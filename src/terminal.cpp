#include "terminal.h"

void Terminal::begin(LGFX_Sprite* canvas, OnKeyboardCallback keyboard_callback)
{
    // Assign member variables
    _canvas = canvas;
    _on_keyboard = keyboard_callback;

    // Clean up some things
    memset(_canon_buffer, '\0', sizeof(_canon_buffer));

    // Prepare the initial terminal state
    clear();
    _render_terminal();
    _dirty = false;
}

void Terminal::update()
{
    unsigned long now = millis();   // Used for keyboard debounce and cursor flashing

    // First check the keyboard
    // The idea being, if we have a key and call the callback,
    // the callback may want to immediately echo the character back to us
    if(_on_keyboard)
    {
        // TODO: Write my own keyboard driver. The M5 one is bulky and cumbersome to use
        if(((now - _last_keypress) > KEYBOARD_UPDATE_INTERVAL) && M5Cardputer.Keyboard.isChange())
        {
            if(M5Cardputer.Keyboard.isPressed())
            {
                Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

                char c = '\0';

                // Process pressed special keys
                if(status.del)
                    c = '\b';
                if(status.enter)
                    c = '\n';

                // Send the special character if one of the special keys was pressed
                if(c != '\0')
                {
                    if(flag_canon)
                        _canon_on_key(c);
                    else
                        _on_keyboard(c);
                }

                // Process pressed normal keys
                for(auto c : status.word)
                {
                    // Control key modifier just masks the bottom 5 bits
                    if(status.ctrl)
                        c &= 0x1f;

                    // Send the character
                    if(flag_canon)
                        _canon_on_key(c);
                    else
                        _on_keyboard(c);
                }
            }

            _last_keypress = now;
        }
    }


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
    // If we're reading in the characters for an escape sequence, don't do anything else
    // but handle processing the escape sequence
    if(_escape_sequence)
    {
        _escape_sequence_cwrite(c);
        return;
    }

    // Not in an escape dequence, just handle the character normally
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
        case '\033':    // Escape
            _escape_sequence = true;
            _escape_buffer_index = 0;
            break;
        default:
            // Did a control character end up here?
            if(c < 0x20)
            {
                // Print it out in a nice fancy way
                cwrite('^');
                cwrite(c|0x40);
            }
            else
            {
                // Anything else, just write the character to the buffer
                _char_buffer[(_cursor_row*COLUMNS) + _cursor_col] = c;
                // and advance to the next position
                _cursor_col++;
                break;
            }
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
    memset(_char_buffer, c, ROWS*COLUMNS);
    // Should this reset the curosr position too?
}

// Set/reset a mode flag for the terminal
void Terminal::set_mode(TerminalFlag flag, bool flag_state)
{
    switch(flag)
    {
        case(FLAG_CANONICAL): flag_canon = flag_state; break;
    }
}


/*
Private Methods
*/

// Called by the keyboard handler if a key is pressed and the canonical mode flag is set
 void Terminal::_canon_on_key(char c)
 {
    cwrite(c);  // Echo the char back to the screen

    // Newline, time to send the buffer out?
    switch(c)
    {
        case('\n'):
            _canon_buffer[_canon_index] = '\n';
            _canon_buffer[_canon_index+1] = '\0';
            _canon_send();
            break;
        case('\b'):
            if(_canon_index > 0)
                _canon_index--;
            else
                cwrite(' ');    // Replace the character the screen just deleted (since it was sent a backspace)
                //_canon_buffer[--_canon_index] = '\0';   // Decrement the index and move the null terminator back
            break;
        case('\03'):  // ETX (ctrl-c))
            _canon_index = 0;
            _on_keyboard(c);
            break;
        default:
            // The buffer is actually CANONICAL_BUFFER_SIZE+1
            // 1 is subtracted to make room for the newline character...
            if(_canon_index < CANONICAL_BUFFER_SIZE-1)
            {
                _canon_buffer[_canon_index++] = c;
            }
            break;
    }
 }

 // Send the canonical buffer out to the connected device.
 // Calls _on_keyboard for each char in the buffer.
 void Terminal::_canon_send()
 {
    for(char c : _canon_buffer)
    {
        if(c == '\0')
        {
            _canon_index = 0;
            return;
        }

        // We're only here if _on_keyboard was set anyways, so I don't think a null check is necessary
        _on_keyboard(c);
    }
 }

 // Render the character buffer to the TFT LCD
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

// Wrap the cursor if it needs to be wrapped
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
        memset(_char_buffer+((COLUMNS*ROWS)-COLUMNS), ' ', COLUMNS);
    }
}

void Terminal::_escape_sequence_cwrite(const char c)
{
    if(_escape_buffer_index == 0)
    {
        // Single-character escape codes, no need to buffer them
        switch(c)
        {
            case '7':   // savecursor
                _cursor_col_mem = _cursor_col;
                _cursor_row_mem = _cursor_row;
                _escape_sequence = false;
                return;
                break;
            case '8':   // restorecursor
                _cursor_row = _cursor_row_mem;
                _cursor_col = _cursor_col_mem;
                _escape_sequence = false;
                return;
                break;
        }

        _escape_buffer[_escape_buffer_index++] = c;
        return;
    }

    if((_escape_buffer_index > 0) && (_escape_buffer[0] == '['))
    {
        if(c)
        switch(c)
        {

        }
    }
}