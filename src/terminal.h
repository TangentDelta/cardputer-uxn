#pragma once

#include <M5Cardputer.h>

#define CANONICAL_BUFFER_SIZE 256

// These are based on the chosen font size and the screen size...
// These should probably be compute automatically eventually
#define SCREEN_DIMS_X 240
#define SCREEN_DIMS_Y 135
#define COLUMNS 30
#define ROWS 16
#define FONT_HEIGHT 8
#define FONT_WIDTH 8
#define CURSOR_BLINK_TIME 200
#define KEYBOARD_UPDATE_INTERVAL 100

// Customizations
#define TERMINAL_COLOR_FG TFT_ORANGE
#define TERMINAL_COLOR_BG TFT_BLACK

using OnKeyboardCallback = std::function<void(const uint8_t)>;

enum TerminalFlag
{
    FLAG_CANONICAL
};

class Terminal
{
public:
    void begin(LGFX_Sprite* canvas, OnKeyboardCallback keyboard_callback);
    void update();
    void cwrite(const char c);
    void print(const char *s);
    void clear(const char c = ' ');
    void set_mode(TerminalFlag flag, bool flag_state);

    bool flag_canon = false;
protected:
    LGFX_Sprite* _canvas         = nullptr;

    // Keyboard
    unsigned long _last_keypress = 0;

    // Cursor
    unsigned long _last_blink = 0;
    bool _cursor_blink = false;
    uint8_t _cursor_row = 0;
    uint8_t _cursor_col = 0;
    uint8_t _cursor_row_mem = 0;
    uint8_t _cursor_col_mem = 0;

    // Canonical mode
    // The canonical mode buffer. It receives characters from the keyboard and immediately echos them.
    // The top bytes is reserved for a null terminator, and the top-1 byte is reserved for a newline character.
    char _canon_buffer[CANONICAL_BUFFER_SIZE+1];
    uint16_t _canon_index = 0;
    void _canon_on_key(char c);
    void _canon_send();

    // Character buffer
    char _char_buffer[COLUMNS * ROWS];
    bool _dirty = false;    // Has the character buffer been modified since the last update?

    // Escape sequence handling
    bool _escape_sequence = false;
    char _escape_buffer_index = 0;
    char _escape_buffer[8];

    OnKeyboardCallback _on_keyboard = nullptr;

    void _render_terminal();
    void _handle_cursor();  // Handle newline wrapping and scrolling the character buffer
    void _escape_sequence_cwrite(const char c);
};