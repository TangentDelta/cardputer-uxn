#include "terminal.h"

void Terminal::begin(LGFX_Sprite* canvas, OnKeyboardCallback keyboard_callback)
{
    // Assign member variables
    _canvas = canvas;
    _on_keyboard = keyboard_callback;

    // Clean up some things
    memset(_canon_buffer, '\0', sizeof(_canon_buffer));

    // Prepare the initial terminal state
    _current_attributes = (_color_default_fg << 3) | _color_default_bg; // Set the background to black and foreground to white
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
                {
                    if(status.fn)
                        _kb_print("\033[3~");   // VT220 forward delete...
                    else
                        c = '\177';
                }
                if(status.enter)
                    c = '\r';

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
                    if(status.fn)
                    {
                        switch(c)
                        {
                            case ';': _kb_print("\033[A"); break; // Up arrow
                            case '.': _kb_print("\033[B"); break; // Down arrow
                            case '/': _kb_print("\033[C"); break; // Right arrow
                            case ',': _kb_print("\033[D"); break; // Left arrow
                            case '`': _kb_print("\033"); break; // Escape
                        }
                    }
                    else
                    {
                        // Non-function key map

                        // Control key modifier just masks the bottom 5 bits
                        if(status.ctrl)
                            c &= 0x1f;

                        // The alt key prepends an escape to the character
                        if(status.alt)
                        {
                            if(flag_canon)
                                _canon_on_key('\033');
                            else
                                _on_keyboard('\033');
                        }

                        // Send the character
                        if(flag_canon)
                            _canon_on_key(c);
                        else
                            _on_keyboard(c);
                    }

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
    if(_escape_state != EscapeState::NORMAL)
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
        case '\177':    // DEL
            if(_cursor_col > 0)
                _cursor_col--;
            else
            {
                if(flag_canon & (_cursor_row > 0))
                {
                    _cursor_col = COLUMNS-1;
                    _cursor_row--;
                }
            }
            _char_buffer[(_cursor_row*COLUMNS) + _cursor_col] = ' ' | (_current_attributes<<8);
            break;
        case '\033':    // Escape
            _escape_state = EscapeState::ESCAPE;
            _escape_params_index = 0;
            break;
        default:
            // Did a control character end up here?
            if(c < 0x20)
            {
                // Print it out in a nice fancy way
                cwrite('^');
                cwrite(c|0x40);
                return; // Return here since the previous two cwrites handled everything
            }
            else
            {
                // Anything else, just write the character to the buffer
                _char_buffer[(_cursor_row*COLUMNS) + _cursor_col] = c | (_current_attributes<<8);
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

void Terminal::char_fill(uint16_t *start, char c, uint16_t count)
{
    for(int i=0; i < count; i++)
        *start++ = c | (_current_attributes<<8);
}

void Terminal::clear(const char c, const uint8_t mode)
{
    _dirty = true;
    switch(mode)
    {
        case(0): char_fill(_char_buffer+(_cursor_col+(_cursor_row*COLUMNS)), c, (ROWS*COLUMNS)-(_cursor_col+(_cursor_row*COLUMNS))); break;  // Mode 0 - From cursor to end of screen
        case(1): char_fill(_char_buffer, c, (_cursor_col+(_cursor_row*COLUMNS))); break; // Mode 1 - From cursor to beginning of screen
        case(2): char_fill(_char_buffer, c, ROWS*COLUMNS); break;  // Mode 2 - Entire screen
        case(4): char_fill(_char_buffer+(_cursor_col+(_cursor_row*COLUMNS)), c, ROWS-_cursor_row); // Mode 4 - Cursor to end of line
        case(5): char_fill(_char_buffer+(_cursor_row*COLUMNS), c, _cursor_col);    // Mode 5 - Start of line to cursor
        case(6): char_fill(_char_buffer+(_cursor_row*COLUMNS), c, COLUMNS);    // Mode 6 - Entire row
    }
    
    // Should this reset the curosr position too?
}

// Set/reset a mode flag for the terminal
void Terminal::set_mode(TerminalFlag flag, bool flag_state)
{
    switch(flag)
    {
        case(TerminalFlag::FLAG_CANONICAL): flag_canon = flag_state; break;
    }
}


/*
Private Methods
*/

// Called by the keyboard handler if a key is pressed and the canonical mode flag is set
 void Terminal::_canon_on_key(char c)
 {
    // Little state machine for handling arrow keys
    if(_canon_escape_state != EscapeState::NORMAL)
    {
        switch(_canon_escape_state)
        {
            case EscapeState::ESCAPE:
                if(c == '[')
                    _canon_escape_state = EscapeState::BRACKET;
                else    // Unrecognized code, return to normal
                    _canon_escape_state = EscapeState::NORMAL;
                break;
            case EscapeState::BRACKET:
                if(c == 'A')    // Up arrow
                {
                    // Backspace the entire input line to handle wrapping back up to the previous line gracefully
                    for(int i=0; i < _canon_index; i++)
                        cwrite('\177');

                    char *p = _canon_buffer_prev;
                    while(*p != '\0')
                        cwrite(*(p++));

                    _canon_index = strlen(_canon_buffer_prev);
                    strcpy(_canon_buffer, _canon_buffer_prev);
                }
                else if(c == 'B')   // Down arrow
                {
                    // Backspace the entire input line to handle wrapping back up to the previous line gracefully
                    for(int i=0; i < _canon_index; i++)
                        cwrite('\177');
                    _canon_index = 0;
                }
                // TODO: Handle left and right arrow keys

                _canon_escape_state = EscapeState::NORMAL;
                break;
        }
        return;
    }

    // Immediately start handling the escape sequence and return if this is an escape character
    if(c == '\033')
    {
        _canon_escape_state = EscapeState::ESCAPE;
        return;
    }

    // If it's not an escape character and we're not handling the escape sequence, carry on...

    // TODO: Make this controllable with a flag
    if(c == '\r') c = '\n'; // Translate the carriage return to a newline

    cwrite(c);  // Echo the char back to the screen

    switch(c)
    {
        case('\n'): // Newline (carriage return)
            _canon_buffer[_canon_index] = '\n'; // Translate it to a newline character
            _canon_buffer[_canon_index+1] = '\0';
            // Copy the buffer so that it can be recalled by an up arrow
            memcpy(_canon_buffer_prev, _canon_buffer, CANONICAL_BUFFER_SIZE);
            _canon_buffer_prev[_canon_index] = '\0';    // We don't want the newline, so null-terminate it

            _canon_send();  // Send the buffer to whatever is listening to the keyboard
            break;
        case('\177'):   // DEL
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
            if(c < 0x20)
                return; // Don't buffer unprintable characters
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

 // Render the character buffer to the LCD
void Terminal::_render_terminal()
{
    _canvas->fillScreen(TERM_COLOR_BLACK);
    _canvas->setFont(&fonts::Font8x8C64);
    _canvas->setTextSize(1);

    // Write each row to the display
    char row_buffer[COLUMNS+1] = {0};
    // Populate the row buffer with the character portion of the character buffer


    uint16_t *p = _char_buffer;
    const uint16_t cursor_index = _cursor_col + (_cursor_row*COLUMNS);
    uint16_t current_char_index = 0;
    for(int row = 0; row < ROWS; row++)
    {
        _canvas->setCursor(0,row*FONT_HEIGHT);
        for(int column=0; column < COLUMNS; column++)
        {
            bool do_blink = _cursor_blink && (row==_cursor_row) && (column==_cursor_col);
            uint16_t c_data = *(p++);
            if(((c_data & 0x8000) == 0)!=do_blink) // XOR the mode with the cursor blink
            {
                // Normal mode (not inverted)
                _canvas->setTextColor(_palette[(c_data>>11)&0xf], _palette[(c_data>>8)&0x7]);
            }
            else
            {
                // Inverted mode
                // How does "bold" apply to inverted colors?
                _canvas->setTextColor(_palette[(c_data>>8)&0x7], _palette[(c_data>>11)&0xf]);
            }

            _canvas->write(c_data&0xff);
        }
    }

    _canvas->pushSprite(0, 0);
}

// Wrap the cursor if it needs to be wrapped
void Terminal::_handle_cursor()
{
    if(_cursor_col >= COLUMNS)
    {
        // Raw mode doesn't wrap to the next line
        if(!flag_canon)
        {
            // So just back and return
            _cursor_col = COLUMNS-1;
            return;
        }
        _cursor_col = 0;
        _cursor_row++;
    }

    if(_cursor_row >= ROWS)
    {
        // Raw mode doesn't scroll either
        if(!flag_canon)
        {
            // So just back the cursor back and return
            _cursor_row = ROWS-1;
            return;
        }
        memcpy(_char_buffer, _char_buffer+COLUMNS, ((COLUMNS*ROWS)-COLUMNS)*2);
        _cursor_row = ROWS-1;
        clear(' ', 6);
    }
}

void Terminal::_kb_print(const char *s)
{
    if(!_on_keyboard)
        return;

    if(flag_canon)
    {
        while(*s != '\0')
            _canon_on_key(*(s++));
    }
    else
    {
        while(*s != '\0')
            _on_keyboard(*(s++));
    }
}

void Terminal::_escape_sequence_cwrite(const char c)
{
    switch(_escape_state)
    {
        case EscapeState::ESCAPE:

            if(c == '[')    // It's bracket time
                _escape_state = EscapeState::BRACKET;
            else if(c == '7')
            {
                _escape_state = EscapeState::NORMAL;
                _cursor_row_mem = _cursor_row;
                _cursor_col_mem = _cursor_col;
            }
            else if(c == '8')
            {
                _escape_state = EscapeState::NORMAL;
                _cursor_row = _cursor_row_mem;
                _cursor_col = _cursor_col_mem;
            }
            else
            {
                // Unrecognized code, return to normal
                _escape_state = EscapeState::NORMAL;
            }
            break;
        case EscapeState::BRACKET:
        case EscapeState::PARAMS:
            // Is it a regular decimal digit or separator?
            if((c >= '0') && (c <= '9') || (c == ';') || (c == '?'))
            {
                // Append it into the parameters string
                if(_escape_params_index < (int)sizeof(_escape_params)-1)
                    _escape_params[_escape_params_index++] = c;
                _escape_state = EscapeState::PARAMS;
            }
            else
            {
                // Not a digit or separator
                _escape_params[_escape_params_index] = '\0';    // Null-terminate the parameters
                _dispatch_escape_sequence(_escape_params, c);   // Perform the sequence's action
                _escape_state = EscapeState::NORMAL;    // Return to normal mode
            }
            break;
    }
}

void Terminal::_dispatch_escape_sequence(const char *params, char c)
{
    int command_args[4] = {0};  // Buffer for up to 4 numeric arguments
    uint8_t arg_count = 0;
    bool is_private = params[0] == '?'; // Private mode flag

    // Parse the params string and populate the arguments for the command
    const char *p = is_private ? params+1 : params; // Skip past the '?' char if private mode
    while((*p != '\0') && arg_count < 4)
    {
        command_args[arg_count++] = atoi(p);    // Try to read the decmial integer
        while((*p != '\0') && (*p != ';')) p++;  // Skip over the digits we just read in  
        if(*p == ';') p++;  // Skip the separator character
    }

    // Args are parsed, time to figure out what command to run

    if(is_private)
    {
        switch(c)
        {
            case 'h':   // DEC private modes
                if(command_args[0] == 1049)
                    _save_terminal_state();
                break;
            case 'l':
                if(command_args[0] == 1049)
                    _restore_terminal_state();
                break;
        }
    }
    else
    {
        switch(c)
        {
            case 'H':   // Home/position cursor
            case 'f':
                _cursor_row = max(0, min(ROWS-1, command_args[0]-1));
                _cursor_col = max(0, min(COLUMNS-1, command_args[1]-1));
                break;
            case 'A':   // Move cursor relative up
                _cursor_row = max(0,(int)_cursor_row - command_args[0]); break;
            case 'B':   // Move cursor relative down
                _cursor_row = min(ROWS-1,_cursor_row + command_args[0]); break;
            case 'C':   // Move cursor relative right
                _cursor_col = min(COLUMNS-1,_cursor_col + command_args[0]); break;
            case 'D':   // Move cursor relative left
                _cursor_col = max(0,(int)_cursor_col - command_args[0]); break;
            case 'J': clear(' ', command_args[0]); break; // Erase screen
            case 'K': clear(' ', command_args[0]+4); break; // Erase line
            case 'm': _escape_color_graphic_handler(command_args, arg_count); break;   // Color/graphics mode
            case 'n':
                if(command_args[0] == 6)    // Cursor position request
                    _send_cursor_position_response();
                break;
        }
    }

}

void Terminal::_send_cursor_position_response()
{
    if(!_on_keyboard)
        return; // Why are we even here...

    char buf[32];
    
    // ESC[#;#R
    sprintf(buf, "\033[%d;%dR", _cursor_row+1, _cursor_col+1);

    // Send it out the keyboard
    _kb_print(buf);
}

void Terminal::_save_terminal_state()
{
    // Don't save the state if we're already in the alternate buffer
    if(_prev_state)
        return;

    _prev_state = new TerminalState;
    _prev_state->cursor_col = _cursor_col;
    _prev_state->cursor_row = _cursor_row;
    memcpy(_prev_state->char_buffer, _char_buffer, ROWS*COLUMNS*sizeof(uint16_t));

    _cursor_row = 0;
    _cursor_col = 0;
    clear();
    _dirty = true;
}

void Terminal::_restore_terminal_state()
{
    if(!_prev_state)
        return;

    _cursor_col = _prev_state->cursor_col;
    _cursor_row = _prev_state->cursor_row;
    memcpy(_char_buffer, _prev_state->char_buffer, ROWS*COLUMNS*sizeof(uint16_t));
    _dirty = true;

    free(_prev_state);
    _prev_state = nullptr;
}

// The color/graphic mode command is complex enough that it dispatches to its own handler
void Terminal::_escape_color_graphic_handler(const int *args, const int arg_count)
{
    for(int i = 0; i < arg_count; i++)
    {
        int arg = args[i];
        uint8_t arg_mode = arg/10;
        uint8_t arg_submode = arg%10;
        switch(arg_mode)
        {
            case(0):    // Graphics mode setters
                switch(arg_submode)
                {
                    case(0): _current_attributes = (_color_default_fg << 3) | _color_default_bg; break;    // Reset all modes
                    case(1): _current_attributes |= 0x40; break;    // Set bold mode
                    case(7): _current_attributes |= 0x80; break;    // Set inverted mode 
                }
                break;
            case(2):    // Graphics mode resetters
                switch(arg_submode)
                {
                    case(1): _current_attributes &= ~0x40; break;    // Reset bold mode
                    case(7): _current_attributes &= ~0x80; break;    // Reset inverted mode 
                }
                break;
            case(3):    // Foreground color
                if(arg_submode>7) arg_submode = _color_default_fg;
                _current_attributes = (_current_attributes&~0x38) | (arg_submode<<3);
                break;
            case(4):    // Background color
                if(arg_submode>7) arg_submode = _color_default_bg;
                _current_attributes = (_current_attributes&~7) | arg_submode;
                break;
        }
    }
}