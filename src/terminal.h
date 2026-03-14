#pragma once

#include <M5Cardputer.h>

#define INPUT_LINE_WIDTH 256
#define PROMPT "> "

// These are based on the chosen font size and the screen size...
// These should probably be compute automatically eventually
#define COLUMNS 30
#define ROWS 15
#define FONT_HEIGHT 8
#define FONT_WIDTH 8
#define CURSOR_BLINK_TIME 200

using OnKeyboardCallback = std::function<void(const uint8_t)>;

class Terminal
{
public:
    void begin(LGFX_Sprite* canvas, OnKeyboardCallback keyboard_callback);
    void update();
    void cwrite(const char c);
    void print(const char *s);
    void clear(const char c = ' ');
protected:
    LGFX_Sprite* _canvas         = nullptr;
    unsigned long _last_blink = 0;
    bool _cursor_blink = false;
    char _char_buffer[COLUMNS * ROWS];
    uint8_t _cursor_row = 0;
    uint8_t _cursor_col = 0;
    bool _dirty = false;    // Has the character buffer been modified since the last update?

    OnKeyboardCallback _on_keyboard = nullptr;

    void _render_terminal();
    void _handle_cursor();  // Handle newline wrapping and scrolling the character buffer
};