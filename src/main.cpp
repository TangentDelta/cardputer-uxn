#include <M5Cardputer.h>
#include <stream_buffer.h>
#include "terminal.h"
#include "uxn.h"
#include "sd_card_handler.h"
#include "wifi_handler.h"
#include "tinyini.h"

#define SHELL_BUFFER_SIZE 64

enum ShellLexerState
{
    IDLE,
    IN_WORD,
    IN_ARGS
};

enum class MetadataParserState
{
    IN_BODY,
    AT_COUNT,
    GET_EXT_ID,
    GET_EXT_VALUE_HI,
    GET_EXT_VALUE_LO,
    FINISHED
};

enum ShellError
{
    ERR_NONE,
    ERR_TOO_MANY_UXN,   // Tried to start too many Uxn instances
    ERR_ROM_NAME_LENGTH,    // ROM name is too long
    ERR_MEMORY, // Out of memory
    ERR_ROM_NOT_FOUND,  // The file for the ROM was not found
    ERR_ROM_SIZE,   // The file for the ROM is too large
    ERR_ROM_LOAD    // The ROM file couldn't be loaded
};

static const char* const shell_error_strings[] = {
    [ShellError::ERR_NONE] = "None",
    [ShellError::ERR_TOO_MANY_UXN] = "Too many instances",
    [ShellError::ERR_ROM_NAME_LENGTH] = "ROM name too long",
    [ShellError::ERR_MEMORY] = "Not enough memory",
    [ShellError::ERR_ROM_NOT_FOUND] = "ROM not found",
    [ShellError::ERR_ROM_SIZE] = "ROM too large",
    [ShellError::ERR_ROM_LOAD] = "Error loading ROM"
};

LGFX_Sprite *canvas;
Terminal terminal;
WiFiHandler wifi_handler;
SDCardHandler sd_card_handler;

uint8_t uxn_instance_index = 0;
Uxn *uxn_instances[8];

const uint8_t metadata_reset_magic[] = {0x80, 0x06, 0x37};

// Shell variables
ShellError shell_error = ShellError::ERR_NONE;
uint8_t shell_buffer_index = 0;
char shell_buffer[SHELL_BUFFER_SIZE];
char uxn_rom_name[20];
ShellLexerState shell_lexer_state = IDLE;

unsigned long last_update = 0;

uint8_t ascii_char_to_nybble(char c)
{
    if(((c < 'G') && (c > '@')) || ((c < 'g') && (c > '`'))) return (c+9)&0xf;
    if((c > '0') && (c < ':')) return c&0xf;
    return 0;
}

void print_heap_free()
{
    int free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    char buffer[20];
    snprintf(buffer, 20, "%d bytes free\n", free);
    terminal.print(buffer);
}

void print_status_char(char status_char, uint8_t color)
{
    char buffer[30];
    snprintf(buffer, 30, "\0337\033[1;30H\033[%dm%c\033[0m\0338", color, status_char);
    terminal.print(buffer);
}

void wifi_failure()
{
    // Put a little red X in the upper-right corner
    print_status_char('X', 31);
}

void wifi_connected()
{
    // Put a little green C in the upper-right corner
    print_status_char('C', 32);
}

void print_shell_error(ShellError e)
{
    terminal.print("\033[1;31m");
    terminal.print(shell_error_strings[e]);
    terminal.print("\033[0m");
}

// Method pre-defs
void shell_print_prompt();

uint8_t get_rom_requirements(File *f)
{
    uint8_t rom_sizing = 0x84;  // Default to a ROM of full size
    // Read the first 6 bytes from the ROM to see if it has metadata
    uint8_t rom_header[6];
    for(int i = 0; i < 6; i++)
    {
        if(f->available())
            rom_header[i] = f->read(); 
    }

    if((rom_header[0] == 0xa0) && (memcmp(rom_header+3, metadata_reset_magic, 3) == 0))
    {
        // The ROM has metadata! Now we need to read it into a buffer
        uint16_t metadata_address = ((rom_header[1] << 8) | rom_header[2]) - 0x100;
        metadata_address++; // Skip past the Varvara version byte
        if(!f->seek(metadata_address)) return 0;

        // Now parse the metadata with a big ol' parser
        MetadataParserState parser_state = MetadataParserState::IN_BODY;
        uint8_t extedned_field_count = 0;
        uint8_t ext_id = 0;
        uint16_t ext_value = 0;

        while(parser_state != MetadataParserState::FINISHED)
        {
            if(!f->available()) return 0;   // No more file? Leave

            char c = f->read();
            switch(parser_state)
            {
                case(MetadataParserState::IN_BODY):
                    // Look for the null terminator for the body
                    if(c=='\0')
                        parser_state = MetadataParserState::AT_COUNT;
                    break;
                case(MetadataParserState::AT_COUNT):
                    // We're at the extended field count, so just read it
                    extedned_field_count = c;
                    // If there are extended fields we need to read them
                    // Otherwise we're done
                    parser_state = extedned_field_count > 0 ? MetadataParserState::GET_EXT_ID : MetadataParserState::FINISHED;
                    break;
                case(MetadataParserState::GET_EXT_ID):
                    ext_id = c;
                    parser_state = MetadataParserState::GET_EXT_VALUE_HI;
                    break;
                case(MetadataParserState::GET_EXT_VALUE_HI):
                    ext_value = (uint16_t)c << 8;
                    parser_state = MetadataParserState::GET_EXT_VALUE_LO;
                    break;
                case(MetadataParserState::GET_EXT_VALUE_LO):
                    ext_value |= c;
                    // Now decode the parameters
                    switch(ext_id)
                    {
                        case(0xf0): // ROM capabilities
                            // aaaa bbbb cccc dddd (bitfield representation)
                            // aaaa = Muxn-style memory size. 1-8, ((2^a) * 256)
                            // bbbb = Muxn-style stack size. 0-4, ((2^b) * 16)
                            // cccc = Muxn-style screen layers. 0=no screen, 1=bg only, 2=bg+fg
                            // dddd = Expansion banks needed. 0=no expansion
                            // Since Cucumber doesn't have the Screen device (or the expansion port)
                            //  all I really care about are a and b, which can be nicely scrunched into a single byte
                            rom_sizing=ext_value>>8;
                    }
                    // Get the next parameter if there are more
                    parser_state = (--extedned_field_count != 0) ? MetadataParserState::GET_EXT_ID : MetadataParserState::FINISHED;
                    break;
            }
        }
    }
        

    // Try to seek back to the start of the file and return with an error if we can't
    // Why wouldn't we be able to? No idea.
    if(!f->seek(0)) return 0;

    return rom_sizing;
}

/*
Quick and dirty Uxn ROM loader
Looks for a ROM with the provided name on the SD card
Loads it into a new Uxn instance if it exists and there are free Uxn instance slots
*/
Uxn *load_rom(const char *rom_name)
{

    // Check if there are Uxn instance slots free
    if(uxn_instance_index == 3)
    {
        shell_error = ShellError::ERR_TOO_MANY_UXN;
        return nullptr;
    }
        

    // Check if ROM name is too long
    if(strlen(rom_name) > 15)
    {
        shell_error = ShellError::ERR_ROM_NAME_LENGTH;
        return nullptr;
    }
        
    // ROM names that start with '!' force the terminal driver into "raw" mode
    bool do_raw = rom_name[0] == '!';
    if(do_raw)  // Skip past the '!' char
        rom_name++;

    // Construct the path for the ROM file to look for
    char rom_path[20];
    snprintf(rom_path, 1+strlen(rom_name)+4+1, "%s.rom", rom_name);

    if(!sd_card_handler.exists(rom_path))
    {
        shell_error = ShellError::ERR_ROM_NOT_FOUND;
        return nullptr;
    }
        

    /*
    TODO: I know the SDcard handler allocates a lot of new data on the heap whenever a file is opened. 
    Will this cause annoying heap fragmentation?
    */
    File f = sd_card_handler.open(rom_path, "r");

    // File doesn't exist
    if(!f)
    {
        shell_error = ShellError::ERR_ROM_NOT_FOUND;
        return nullptr;
    }

    // File is larger than Uxn RAM
    if(f.size() > 0xFF00)
    {
        // TODO: This code is repeated a lot here. Could this be a macro?
        f.close();
        shell_error = ShellError::ERR_ROM_SIZE;
        return nullptr;
    }
        
    uint8_t rom_sizing = get_rom_requirements(&f);
    if(rom_sizing == 0x00)
    {
        f.close();
        shell_error = ShellError::ERR_ROM_LOAD;
        return nullptr;
    }

    // Add a new Uxn instance
    // TODO: Automatically size this instance based on the size of the ROM? ROM metadata?
    Uxn *u = new Uxn(rom_sizing>>4, rom_sizing&0xf);
    // Initialize the instance
    if(!u->begin())
    {
        // If there's not enough memory, immedaitely delete the new instance and return a nullptr
        f.close();
        delete u;
        shell_error = ShellError::ERR_MEMORY;
        return nullptr;
    }
    uxn_instances[uxn_instance_index++] = u;

    // Load the contents of the ROM into the instance's RAM
    uint16_t load_addr = 0x100;
    while(f.available())
    {
        uint8_t c = f.read();
        u->mem_poke(load_addr++ , c);
    }

    f.close();

    // Give the new Uxn instance a reference to the SD card handler
    // This will eventually be a more general filesystem handler...
    u->sd_card_handler = sd_card_handler;

    // tell it about the status of the wifi
    bool wifi_okay = wifi_handler.get_status() == 0x03;
    u->wifi_connected = wifi_okay;
    u->dev_poke(74, wifi_okay ? 0x01 : 0x80);

    if(do_raw)
        terminal.set_mode(TerminalFlag::FLAG_CANONICAL, false);

    return u;
}

void terminal_cwrite(uint8_t value)
{
    terminal.cwrite(value);
}

// Sets the mode flags of the terminal based on the flag byte from Uxn
void terminal_uxn_stty(uint8_t value)
{
    terminal.set_mode(TerminalFlag::FLAG_CANONICAL, (value & 0x40) == 0);
}

// Wires the Console I/O between Uxn instances
void wire_uxn_instances()
{
    if(uxn_instance_index == 0)
        return;

    if(uxn_instance_index > 1)
    {
        for(int i = 0; i < (uxn_instance_index-1); i++)
        {
            Uxn *ua = uxn_instances[i];
            Uxn *ub = uxn_instances[i+1];
            ua->set_deo_callback(0x18, [ub](uint8_t value){ ub->console_vector(value); });
        }
    }

    // Set up the first instance for setting tty flags
    uxn_instances[0]->dev_poke(0x16, 0x80);
    uxn_instances[0]->set_deo_callback(0x16, terminal_uxn_stty);

    // Wire up the last instance to the terminal's screen
    uxn_instances[uxn_instance_index-1]->set_deo_callback(0x18, terminal_cwrite);
}

// Relases all of the uxn instances
void release_uxn_instances()
{

    for(int i = 0; i < uxn_instance_index; i++)
    {
        // Tell the instance that it is being shut down
        if(uxn_instances[i]->console_vector_set)
            uxn_instances[i]->console_vector(0x0a, Uxn::ConsoleType::type_argument_end);

        delete uxn_instances[i];
    }

    // TODO: Give the terminal a "restore mode" method?
    terminal.set_mode(TerminalFlag::FLAG_CANONICAL, true);

    uxn_instance_index = 0;
}

// Check all of the Uxn instances to make sure they're all alive
void check_uxn_instances()
{
    for(int i=0; i < uxn_instance_index; i++)
    {
        if(!uxn_instances[i]->alive)
        {
            release_uxn_instances();
            terminal.cwrite('\n');
            shell_print_prompt();
        }
        else
        {
            // Instance is alive, run its housekeeping method
            uxn_instances[i]->update();
        }
    }
}

// Prints the prompt for the shell
// Includes the number of bytes free on the heap
void shell_print_prompt()
{
    print_heap_free();
    terminal.print("\033[32m>\033[0m ");
}

// Set up a new Uxn instance given a word from the shell lexer
// Returns true if the instance was created successfully
bool shell_start_instance(const char *shell_word)
{
    Uxn *new_uxn = load_rom(shell_word);
    if(new_uxn == nullptr)
    {
        terminal.print(shell_word);
        // Issue loading the Uxn instance
        if(shell_error != ShellError::ERR_NONE)
        {
            // If an error code was provided, print it out
            terminal.print(": ");
            print_shell_error(shell_error);
            terminal.cwrite('\n');
            shell_error = ShellError::ERR_NONE;
        }
        else
        {
            // Otherwise just print the name of the offending ROM (but frame is as a sarcastic question)
            terminal.print("?\n");
        }
        
        release_uxn_instances();
        return false;
    }

    new_uxn->set_deo_callback(0x19, terminal_cwrite);   // Set up the Console/error callback
    return true;
}

bool shell_start_redirect()
{
    return shell_start_instance("redirect");
}

/*
Shell buffer processor
*/
void shell_process_buffer()
{
    char shell_word[20];
    char shell_word_index = 0;
    uint8_t arg_stack_index = 0;
    char *arg_stack[16];
    
    shell_lexer_state = IDLE;
    shell_buffer_index = 0;
    bool lexing = true;
    // Shell lexer
    while(lexing)
    {
        char c = shell_buffer[shell_buffer_index++];

        switch(shell_lexer_state)
        {
            // Idle state
            // Skip over any space characters
            case IDLE:
                switch(c)
                {
                    case '|':
                    case '>':
                    case ' ':
                        break;
                    case '\n':
                    case '\0':
                        lexing = false;
                        break;
                    default:
                        shell_word_index = 0;
                        shell_word[shell_word_index++] = c;
                        shell_lexer_state = IN_WORD;
                        break;
                }

                break;
            // In word state
            case IN_WORD:
                switch(c)
                {
                    // Immediately hitting these chars causes the same initial effect
                    case '>':
                    case '\n':
                    case '\0':
                    case '|':
                        shell_word[shell_word_index] = '\0';    // Null-terminate the word
                        if(!shell_start_instance(shell_word))   // Try starting an instance with the provided word
                            return;
                        arg_stack[arg_stack_index++] = nullptr;    // Since we immediately hit this char, this word has no args
                        break;
                    case ' ':   // The space isn't exactly the same
                        shell_word[shell_word_index] = '\0';    // Null-terminate the word
                        if(!shell_start_instance(shell_word))   // Try starting an instance with the provided word
                            return;
                        arg_stack[arg_stack_index++] = shell_buffer+(shell_buffer_index-1);    // Store the start of this instance's args
                        shell_lexer_state = IN_ARGS;    // Start gathering the arguments for the word
                        break;
                    default:    // Just a plain printable character. 
                        // Append it to the word we're building
                        shell_word[shell_word_index++] = c;
                        break;
                }

                // Now handle the special character behaviors
                switch(c)
                {
                    case '\n':
                    case '\0':
                        lexing = false; // Stop the lexer
                        break;
                    case '|':
                        shell_lexer_state = IDLE;   // Skip past any spaces that might come after the pipe
                        break;
                    case '>':
                        if(!shell_start_redirect()) // Start a redirect instance
                            return;
                        arg_stack[arg_stack_index++] = shell_buffer+(shell_buffer_index-1);    // Store the start of the redirect's args (itself)
                        shell_lexer_state = IN_ARGS;    // Grab the arguments for the redirect
                        break;
                }
                break;
            // In argument state
            // Just blindly moves ahead while checking for the start of a new word
            case IN_ARGS:
                switch(c)
                {
                    case '\n':
                    case '\0':
                        lexing = false; // Stop the lexer
                        break;
                    case '|':
                        shell_lexer_state = IDLE;
                        break;
                    case '>':
                        if(!shell_start_redirect()) // Start a redirect instance
                            return;
                        arg_stack[arg_stack_index++] = shell_buffer+(shell_buffer_index-1);    // Store the start of the redirect's args (itself)
                        break;
                    default:
                        break;
                }
                break;
        }
    }

    if(uxn_instance_index > 0)
    {
        wire_uxn_instances();   // Wire the console devices together

        // Evaluate the Uxn instances from right to left
        // This gives the instances time to initialize their vectors
        for(int i = uxn_instance_index-1; i >= 0; i--)
        {
            Uxn *u = uxn_instances[i];

            char *instance_args = arg_stack[i];
            // If the instance has no arguments, skip this instance
            if(instance_args == nullptr)
            {
                u->dev_poke(0x17, 0x00);    // Indicate to the VM that there are no arguments (null -> Console/type)
                u->eval(0x100); // Process the reset vector
                continue;   // Skip sending the args
            }

            u->dev_poke(0x17, 0x01);    // Indicate to the VM that there are args
            u->eval(0x100); // Process the reset vector

            // TODO: This should get made into a lexer
            bool space_skip = true;    // Flag to skip redundant spaces
            bool first_printable = false;
            uint8_t arg_count = 0;
            for(int i = 0; i < 32; i++)
            {
                char c = instance_args[i];
                // Get rid of any leading spaces
                if((c == ' ') && space_skip)
                    continue;

                // Redirects point to themselves for args, so we need to skip past them
                if((i == 0) && (c == '>'))
                    continue;

                // Stop processing args if we hit a pipe, redirect, or the end of the buffer
                if((c == '|') || (c == '>') || (c == '\0') || (c == '\n'))
                {
                    // Only terminate the arguments if there are actually arguments!
                    if(arg_count > 0)
                        u->console_vector(0xa, Uxn::ConsoleType::type_argument_end);
                    break;
                }

                // If this is a space, start skipping spaces
                if(c == ' ')
                {
                    space_skip = true;  // Skip past any future spaces
                    continue;
                }

                // If we were skipping spaces and now are not, send a spacer before sending the argument character
                if(space_skip)
                {
                    arg_count++;
                    if(first_printable)
                        u->console_vector(0xa, Uxn::ConsoleType::type_argument_spacer);
                    else
                        first_printable = true;

                    space_skip = false;
                }

                u->console_vector(c, Uxn::ConsoleType::type_argument);
            }
        }

        // And now, make sure the instances are all alive
        for(int i = 0; i < uxn_instance_index; i++)
        {
            Uxn *u = uxn_instances[i];
            if(!u->alive)
            {
                // Uh oh, this instance has no console vector set
                // Clean everything up and immediately return
                release_uxn_instances();
                terminal.cwrite('\n');  // A nice newline for the road
                return;
            }
        }
    }
}

/*
Shell key handler
*/
void shell_key_handler(const uint8_t c)
{
    switch(c)
    {
        case('\n'):
            shell_process_buffer();
            shell_buffer_index = 0;
            memset(shell_buffer, '\0', SHELL_BUFFER_SIZE);
            if(uxn_instance_index == 0)
                shell_print_prompt();
            break;
        default:
            if(shell_buffer_index < SHELL_BUFFER_SIZE)
                shell_buffer[shell_buffer_index++] = c;
            break;
    }
}

/*
Called by the terminal when a key is pressed
Routes the keypress to either the shell key handler or the bottom-most Uxn instance
*/
void shell_on_key(const uint8_t c)
{
    if(uxn_instance_index > 0)
    {
        if(c == '\03')  // ETX (ctrl-c)
        {
            release_uxn_instances();
            terminal.cwrite('\n');
            shell_print_prompt();
            return;
        }

        uxn_instances[0]->console_vector(c);
    }
    else
    {
        shell_key_handler(c);
    }
}

// Attempts to load settings from "/settings.ini" off of the SD card
void load_settings()
{
    // Some quick sanity checks before we try loading the file
    if(!sd_card_handler.okay)
    {
        print_status_char('S', 31); // Red S
        return;
    }

    if(!sd_card_handler.exists("/settings.ini"))
    {
        print_status_char('i', 33); // Yello i
        return;
    }

    File f = sd_card_handler.open("/settings.ini", "r");
    if(!f)
    {
        print_status_char('i', 33); // Yellow i
        f.close();
        return;
    }

    TinyINI<2,4,32> ini;

    uint8_t section_wifi = ini.register_section("wifi");
    ini.register_key(section_wifi, "ssid", [](const char *s){ wifi_handler.set_ssid(s); });
    ini.register_key(section_wifi, "password", [](const char *s){ wifi_handler.set_password(s); });

    while(f.available())
    {
        if(ini.parse(f.read()) != TinyINIStatus::OKAY)
        {
            print_status_char('i', 31); // Red i
            f.close();
            return;

        }
    }
    ini.finish();
    f.close();
}

void setup()
{
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    // Set up WiFi
    wifi_handler.begin();
    wifi_handler.on_connect_fail(wifi_failure);
    wifi_handler.on_connect_success(wifi_connected);

    // Set up the screen and the terminal
    canvas = new LGFX_Sprite(&M5Cardputer.Display);
    canvas->createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
    terminal.begin(canvas, [](const uint8_t c){shell_on_key(c);});
    terminal.set_mode(TerminalFlag::FLAG_CANONICAL, true);

    // Initialize the SD card handler
    sd_card_handler.begin();

    // Load the settings
    load_settings();

    terminal.print("\033[1;32mCucumber \033[21;33m");
    terminal.print(GIT_COMMIT);
    terminal.print("\n\033[32mCardputer Uxn Environment\033[0m\n");
    shell_print_prompt();
}

void loop()
{
    check_uxn_instances();
    M5Cardputer.update();
    wifi_handler.update();
    terminal.update();
}