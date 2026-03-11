#include <M5Cardputer.h>
#include "terminal.h"
#include "uxn.h"

LGFX_Sprite *canvas;
Terminal terminal;
Uxn uxn;

uint8_t hello_rom[] = {
0xa0,0x01,0x12,0x94,0x06,0x20,0x00,0x03,0x02,0x22,0x00,0x80,0x18,0x17,0x21,0x40,
0xff,0xf1,0x48,0x65,0x6c,0x6c,0x6f,0x20,0x57,0x6f,0x72,0x6c,0x64,0x21,0x00 
};

unsigned long last_update = 0;

void setup()
{
    M5Cardputer.begin();

    canvas = new LGFX_Sprite(&M5Cardputer.Display);
    canvas->createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());

    terminal.begin(canvas);

    uxn.set_deo_callback(0x18, [](uint8_t value){terminal.cwrite(value);});
    uxn.load(hello_rom, sizeof(hello_rom));

    uxn.eval(0x100);
}

void loop()
{
    M5Cardputer.update();
    terminal.update();

    /*
    if((millis() - last_update) > 100)
    {
        terminal.print("Test\n");
        last_update = millis();
    }*/
}