#include <M5Cardputer.h>
#include <stream_buffer.h>
#include "terminal.h"
#include "uxn.h"
#include "sd_card_handler.h"

#define CLI_BUFFER_SIZE 64

LGFX_Sprite *canvas;
Terminal terminal;
SDCardHandler sd_card_handler;

uint8_t uxn_instance_index = 0;
Uxn *uxn_instances[8];

uint8_t cli_buffer_index = 0;
char cli_buffer[CLI_BUFFER_SIZE];
char uxn_rom_name[20];

unsigned long last_update = 0;

void print_heap_free()
{
    int free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    terminal.print(String(free).c_str());
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

    // TODO: Wire up the first Uxn instance to the terminal's keyboard

    // Wire up the last instance to the terminal's screen
    uxn_instances[uxn_instance_index-1]->set_deo_callback(0x18, terminal_cwrite);
}

// Relases all of the uxn instances
void release_uxn_instances()
{
    for(int i = 0; i < uxn_instance_index; i++)
    {
        delete uxn_instances[i];
    }

    uxn_instance_index = 0;
}

void cli_print_prompt()
{
    print_heap_free();
    terminal.print("> ");
}

/*
Placeholder CLI processor
Quick and dirty, and I really hate it.
*/
void cli_process_buffer()
{
    bool loading_args = false;  // Set when a program name has been encountered
    char cli_word[20];
    char cli_word_index = 0;
    for(int i = 0; i < CLI_BUFFER_SIZE; i++)
    {
        char c = cli_buffer[i];
        if(!((c == ' ') | (c == '\0')))
        {
            cli_word[cli_word_index++] = c;
        }
        else
        {
            cli_word[cli_word_index] = '\0';
            int cli_word_length = strlen(cli_word);
            if(cli_word_length > 0)
            {
                // Special single-character words
                if(cli_word_length == 1)
                {
                    if(cli_word[0] == '|')
                    {
                        // Todo: handle pipe operation
                        continue;
                    }
                }

                if(!loading_args)
                {
                    Uxn *new_uxn = load_rom(cli_word);
                    if(new_uxn == nullptr)
                        terminal.print("Load error\n");
                }

            }

            cli_word_index = 0; // Reset the word index for the next word

            // If we're at the end, stop looping through the buffer
            if(c == '\0')
                break;
        }
    }

    if(uxn_instance_index > 0)
    {
        wire_uxn_instances();   // Wire the console devices together

        terminal.cwrite('\n');
        uxn_instances[0]->eval(0x100);

        release_uxn_instances();
    }
}

// Called by the terminal when a key is pressed
void cli_on_key(const uint8_t c)
{
    terminal.cwrite(c); // Echo it back

    switch(c)
    {
        case('\b'):
            if(cli_buffer_index > 0)
                cli_buffer[--cli_buffer_index] = '\0';
            break;
        case('\n'):
            cli_process_buffer();
            cli_buffer_index = 0;
            memset(cli_buffer, '\0', CLI_BUFFER_SIZE);
            cli_print_prompt();
            break;
        default:
            if(cli_buffer_index < CLI_BUFFER_SIZE)
                cli_buffer[cli_buffer_index++] = c;
            break;
    }
}

void setup()
{
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    // Set up the screen and the terminal
    canvas = new LGFX_Sprite(&M5Cardputer.Display);
    canvas->createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
    terminal.begin(canvas, [](const uint8_t c){cli_on_key(c);});

    // Initialize the SD card handler
    sd_card_handler.begin();

    cli_print_prompt();
}

void loop()
{
    M5Cardputer.update();
    terminal.update();
}