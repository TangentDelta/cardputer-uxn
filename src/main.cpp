#include <M5Cardputer.h>
#include <stream_buffer.h>
#include "terminal.h"
#include "uxn.h"
#include "sd_card_handler.h"

#define SHELL_BUFFER_SIZE 64

enum ShellLexerState
{
    IDLE,
    IN_WORD,
    IN_ARGS
};

LGFX_Sprite *canvas;
Terminal terminal;
SDCardHandler sd_card_handler;

uint8_t uxn_instance_index = 0;
Uxn *uxn_instances[8];

// Shell variables
uint8_t shell_buffer_index = 0;
char shell_buffer[SHELL_BUFFER_SIZE];
char uxn_rom_name[20];
ShellLexerState shell_lexer_state = IDLE;

unsigned long last_update = 0;

void print_heap_free()
{
    int free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    terminal.print(String(free).c_str());
    terminal.print(" bytes free");
    terminal.cwrite('\n');
}

/*
Quick and dirty Uxn ROM loader
Looks for a ROM with the provided name on the SD card
Loads it into a new Uxn instance if it exists and there are free Uxn instance slots
*/
Uxn *load_rom(const char *rom_name)
{

    // Check if there are Uxn instance slots free
    // TODO: Also check to make sure there is enough heap to allocate to a new Uxn instance
    if(uxn_instance_index == 7)
        return nullptr;

    // Check if ROM name is too long
    if(strlen(rom_name) > 15)
        return nullptr;

    // Construct the path for the ROM file to look for
    char rom_path[20];
    snprintf(rom_path, 1+strlen(rom_name)+4+1, "/%s.rom", rom_name);

    if(!sd_card_handler.exists(rom_path))
        return nullptr;

    /*
    TODO: I know the SDcard handler allocates a lot of new data on the heap whenever a file is opened. 
    Will this cause annoying heap fragmentation?
    */
    File f = sd_card_handler.open(rom_path, "r");

    // File doesn't exist
    if(!f)
        return nullptr;

    // File is larger than Uxn RAM
    if(f.size() > 0xF000)
        return nullptr;

    // Add a new Uxn instance
    // TODO: Automatically size this instance based on the size of the ROM? ROM metadata?
    Uxn *u = new Uxn();
    uxn_instances[uxn_instance_index++] = u;

    // Load the contents of the ROM into the instance's RAM
    uint16_t load_addr = 0x100;
    while(f.available())
    {
        uint8_t c = f.read();
        u->mem_poke(load_addr++ , c);
    }

    f.close();

    return u;
}

void terminal_cwrite(uint8_t value)
{
    terminal.cwrite(value);
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

    // TODO: Wire up the first Uxn instance to the terminal's keyboard

    // Wire up the last instance to the terminal's screen
    uxn_instances[uxn_instance_index-1]->set_deo_callback(0x18, terminal_cwrite);
}

// Relases all of the uxn instances
void release_uxn_instances()
{
    // TODO: When I eventually add the file device, this needs to clean up floating file handles too
    for(int i = 0; i < uxn_instance_index; i++)
    {
        delete uxn_instances[i];
    }

    uxn_instance_index = 0;
}

// Prints the prompt for the shell
// Includes the number of bytes free on the heap
void shell_print_prompt()
{
    print_heap_free();
    terminal.print("> ");
}

// Set up a new Uxn instance given a word from the shell lexer
// Returns true if the instance was created successfully
bool shell_start_instance(const char *shell_word)
{
    Uxn *new_uxn = load_rom(shell_word);
    if(new_uxn == nullptr)
    {
        // Issue loading the Uxn instance.
        terminal.print(shell_word);
        terminal.print("?\n");
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
    bool loading_args = false;  // Set when a program name has been encountered
    bool space_skip = true; // Ignore space characters
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
            // In argument state
            // Just blindly moves ahead while checking for the start of a new word
            case IN_ARGS:
                switch(c)
                {
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
            u->eval(0x100); // Process the reset vector

            // Pass in the instance's arguments
            char *instance_args = arg_stack[i];
            // If the instance has no arguments, skip this instance
            if(instance_args == nullptr)
                continue;

            // TODO: This should get made into a lexer
            bool space_skip = true;    // Flag to skip redundant spaces
            bool first_printable = false;
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
                if((c == '|') || (c == '>') || (c == '\0'))
                {
                    u->console_vector(0xa, ConsoleType::type_argument_end);
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
                    if(first_printable)
                        u->console_vector(0xa, ConsoleType::type_argument_spacer);
                    else
                        first_printable = true;

                    space_skip = false;
                }

                u->console_vector(c, ConsoleType::type_argument);
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
    terminal.cwrite(c); // Echo it back

    switch(c)
    {
        case('\b'):
            if(shell_buffer_index > 0)
                shell_buffer[--shell_buffer_index] = '\0';
            break;
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

void setup()
{
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    // Set up the screen and the terminal
    canvas = new LGFX_Sprite(&M5Cardputer.Display);
    canvas->createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
    terminal.begin(canvas, [](const uint8_t c){shell_on_key(c);});

    // Initialize the SD card handler
    sd_card_handler.begin();

    terminal.print("Cardputer Uxn Environment\n");
    shell_print_prompt();
}

void loop()
{
    M5Cardputer.update();
    terminal.update();
}