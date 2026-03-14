#include "terminal.h"

void Terminal::begin(LGFX_Sprite* canvas, OnKeyboardCallback keyboard_callback)
{
    // Assign member variables
    _canvas = canvas;
    _on_keyboard = keyboard_callback;

    // Prepare the initial terminal state
    clear();
    _render_terminal();
    _dirty = false;
}

void Terminal::update()
{
    // First check the keyboard
    // The idea being, if we have a key and call the callback,
    // the callback may want to immediately echo the character back to us
    if(_on_keyboard)
    {
        if(M5Cardputer.Keyboard.isChange())
        {
            if(M5Cardputer.Keyboard.isPressed())
            {
                Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

                // Process pressed special keys
                if(status.del)
                    _on_keyboard('\b');
                if(status.enter)
                    _on_keyboard('\n');

                // Process pressed keys
                for(auto c : status.word)
                {
                    if(status.ctrl)
                    {
                        c &= 0x1f;
                    }

                    _on_keyboard(c);
                }
            }
        }
    }


    unsigned long now = millis();
    if((now - _last_blink) > CURSOR_BLINK_TIME)
    {
        _last_blink = now;
        _cursor_blink = !_cursor_blink;
        _dirty = true;
    }
    // Re-render the terminal if the character buffer has been modified
    // or it's time to invert the cursor
    if(_dirty)
    {
        _render_terminal();
        _dirty = false;
    }
}

void Terminal::cwrite(const char c)
{
    _dirty = true;  // Flag the character buffer as being modified
    switch(c)
    {
        case '\n':  // Newline
            // Advance to the next row
            _cursor_row++;
            // The carriage return feels kinda wrong here, but if it poses to
            // be a problem we'll cross that bridge when we come to it
            _cursor_col = 0;
            break;
        case '\r':
            _cursor_col = 0;
            break;
        case '\b':
            if(_cursor_col > 0)
                _cursor_col--;
            else
            {
                if(_cursor_row > 0)
                {
                    _cursor_col = COLUMNS-1;
                    _cursor_row--;
                }
            }
            _char_buffer[(_cursor_row*COLUMNS) + _cursor_col] = ' ';
            break;
        default:
            // Anything else, just write the character to the buffer
            _char_buffer[(_cursor_row*COLUMNS) + _cursor_col] = c;
            // and advance to the next position
            _cursor_col++;
            break;
    }

    _handle_cursor();   // Advance to the next line or scroll as needed
}

void Terminal::print(const char *s)
{
    // Print up to 256 characters
    for(int i = 0; i < 256; i++)
    {
        char c = s[i];
        // If we've reached the null terminator, immediately return
        if(c == '\0')
            return;

        cwrite(c);
    }
    
}

void Terminal::clear(const char c)
{
    _dirty = true;
    for(int i = 0; i < ROWS * COLUMNS; i++)
    {
        _char_buffer[i] = c;
    }
}

void Terminal::_handle_cursor()
{
    if(_cursor_col >= COLUMNS)
    {
        _cursor_col = 0;
        _cursor_row++;
    }

    if(_cursor_row >= ROWS)
    {
        memcpy(_char_buffer, _char_buffer+COLUMNS, (COLUMNS*ROWS)-COLUMNS);
        _cursor_row = ROWS-1;
    }
}

void Terminal::_render_terminal()
{
    _canvas->fillScreen(TFT_BLACK);
    _canvas->setFont(&fonts::Font8x8C64);
    _canvas->setTextSize(1);
    _canvas->setBaseColor(TFT_BLACK);
    _canvas->setTextColor(TFT_WHITE, TFT_BLACK);

    // Write each row to the display
    char row_buffer[COLUMNS+1];
    row_buffer[COLUMNS] = '\0';

    for(int row = 0; row < ROWS; row++)
    {
        int i = row * COLUMNS;
        memcpy(row_buffer, _char_buffer+i, COLUMNS);
        _canvas->setCursor(0,row*FONT_HEIGHT);
        _canvas->print(row_buffer);
    }

    // Now render the cursor
    _canvas->setColor(_cursor_blink ? TFT_WHITE : TFT_BLACK);
    _canvas->drawRect(_cursor_col*FONT_WIDTH, _cursor_row*FONT_HEIGHT, FONT_WIDTH, FONT_HEIGHT);

    _canvas->pushSprite(0, 0);
}