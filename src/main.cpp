#include <M5Cardputer.h>
#include <stream_buffer.h>
#include "terminal.h"
#include "uxn.h"
#include "sd_card_handler.h"

#define CLI_BUFFER_SIZE 64

LGFX_Sprite *canvas;
Terminal terminal;
SDCardHandler sd_card_handler;
Uxn uxn;

uint8_t hello_rom[] = {
0xa0,0x01,0x12,0x94,0x06,0x20,0x00,0x03,0x02,0x22,0x00,0x80,0x18,0x17,0x21,0x40,
0xff,0xf1,0x48,0x65,0x6c,0x6c,0x6f,0x20,0x57,0x6f,0x72,0x6c,0x64,0x21,0x00 
};

uint8_t cli_buffer_index = 0;
char cli_buffer[CLI_BUFFER_SIZE];
char uxn_rom_name[20];

unsigned long last_update = 0;

void load_rom_file(const char *file_name, Uxn *u)
{
    File f = sd_card_handler.open(file_name, "r");
    if(!f)
        return;

    uint16_t load_addr = 0x100;
    while(f.available())
    {
        uint8_t c = f.read();
        u->mem_poke(load_addr, c);
        // Increment and handle overflow past 0xffff
        if(++load_addr == 0)
        {
            f.close();
            return;
        }
    }

    f.close();
}

void cli_process_buffer()
{
    bool loading_args = false;
    bool rom_loaded = false;
    int word_start = 0;
    for(int i = 0; i < CLI_BUFFER_SIZE; i++)
    {
        char c = cli_buffer[i];
        if((c == ' ') | (c == '\0'))
        {
            int word_size = i - word_start;
            if(word_size > 0)
            {
                if(word_size == 1)
                {
                    if(cli_buffer[i-1] == '|')
                    {
                        // Handle pipe operation
                        continue;
                    }
                }

                if(!loading_args)
                {
                    uxn_rom_name[0] = '/';
                    memcpy(uxn_rom_name+1, cli_buffer+word_start, word_size);
                    strcpy(uxn_rom_name+word_size+1, ".rom");
                    terminal.print(uxn_rom_name);
                    if(sd_card_handler.exists(uxn_rom_name))
                    {
                        load_rom_file(uxn_rom_name, &uxn);
                        rom_loaded = true;
                    }
                    else
                        terminal.print("\nFile not found\n");
                }

            }
            if(c == '\0')
                break;
        }
    }

    if(rom_loaded)
    {
        terminal.cwrite('\n');
        uxn.eval(0x100);
    }
}

void cli_on_key(const uint8_t c)
{
    terminal.cwrite(c);

    switch(c)
    {
        case('\b'):
            if(cli_buffer_index > 0)
                cli_buffer[--cli_buffer_index] = '\0';
            break;
        case('\n'):
            cli_process_buffer();
            cli_buffer_index = 0;
            cli_buffer[0] = '\0';
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

    uxn.set_deo_callback(0x18, [](uint8_t value){terminal.cwrite(value);});
    //uxn.load(hello_rom, sizeof(hello_rom));

    //uxn.eval(0x100);
}

void loop()
{
    M5Cardputer.update();
    terminal.update();
}