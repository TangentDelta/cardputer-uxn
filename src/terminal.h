#pragma once

#include <M5Cardputer.h>

#define CANONICAL_BUFFER_SIZE 128

// These are based on the chosen font size and the screen size...
// These should probably be compute automatically eventually
#define SCREEN_DIMS_X 240
#define SCREEN_DIMS_Y 135
#define FONT_HEIGHT 8
#define FONT_WIDTH 8
#define COLUMNS (SCREEN_DIMS_X / FONT_WIDTH)
#define ROWS (SCREEN_DIMS_Y / FONT_HEIGHT)
#define CURSOR_BLINK_TIME 200
#define KEYBOARD_UPDATE_INTERVAL 100

// The TFT uses a 16-bit color code
//   5      6     5
// RRRRR GGGGGG BBBBB
// You can find the colors here https://doc-tft-espi.readthedocs.io/tft_espi/colors/

// Standard palette.

#define TERM_COLOR_BLACK       0x0000
#define TERM_COLOR_RED         0x8000
#define TERM_COLOR_GREEN       0x0400
#define TERM_COLOR_YELLOW      0x8400
#define TERM_COLOR_BLUE        0x0010
#define TERM_COLOR_MAGENTA     0x8010
#define TERM_COLOR_CYAN        0x0410
#define TERM_COLOR_WHITE       0xc618
#define TERM_COLOR_BRIGHT_BLACK   0x8410
#define TERM_COLOR_BRIGHT_RED     0xf800
#define TERM_COLOR_BRIGHT_GREEN   0x07e0
#define TERM_COLOR_BRIGHT_YELLOW  0xffe0
#define TERM_COLOR_BRIGHT_BLUE    0x001f
#define TERM_COLOR_BRIGHT_MAGENTA 0xf81f
#define TERM_COLOR_BRIGHT_CYAN    0x07ff
#define TERM_COLOR_BRIGHT_WHITE   0xffff


// Retro "amber" color palette
/*
#define TERM_COLOR_BLACK          0x0000
#define TERM_COLOR_RED            0xa000
#define TERM_COLOR_GREEN          0x0580
#define TERM_COLOR_YELLOW         0x8be0
#define TERM_COLOR_BLUE           0x0008
#define TERM_COLOR_MAGENTA        0x8008
#define TERM_COLOR_CYAN           0x0580
#define TERM_COLOR_WHITE          0x8b00

#define TERM_COLOR_BRIGHT_BLACK   0x8be0
#define TERM_COLOR_BRIGHT_RED     0xf800
#define TERM_COLOR_BRIGHT_GREEN   0x07e0
#define TERM_COLOR_BRIGHT_YELLOW  0xfd20
#define TERM_COLOR_BRIGHT_BLUE    0x001f
#define TERM_COLOR_BRIGHT_MAGENTA 0xf810
#define TERM_COLOR_BRIGHT_CYAN    0x07f0
#define TERM_COLOR_BRIGHT_WHITE   0xfd20
*/

using OnKeyboardCallback = std::function<void(const uint8_t)>;

enum class TerminalFlag
{
    FLAG_CANONICAL
};

enum class EscapeState
{
    NORMAL,
    ESCAPE, // Escape character '\033' encountered
    BRACKET,    // Escape '[' encountered 
    PARAMS  // Accumulating parameters
};

enum ANSIColors
{
    ANSI_COLOR_BLACK,
    ANSI_COLOR_RED,
    ANSI_COLOR_GREEN,
    ANSI_COLOR_YELLOW,
    ANSI_COLOR_BLUE,
    ANSI_COLOR_MAGENTA,
    ANSI_COLOR_CYAN,
    ANSI_COLOR_WHITE,
    ANSI_COLOR_DEFAULT
};

struct TerminalState
{
    uint8_t cursor_row = 0;
    uint8_t cursor_col = 0;
    uint16_t char_buffer[COLUMNS * ROWS];
};

class Terminal
{
public:
    void begin(LGFX_Sprite* canvas, OnKeyboardCallback keyboard_callback);
    void update();
    void cwrite(const char c);
    void print(const char *s);
    void char_fill(uint16_t *start, char c, uint16_t count);
    void clear(const char c = ' ', const uint8_t mode = 2);
    void set_mode(TerminalFlag flag, bool flag_state);
    void set_palette(uint8_t palette_index, uint16_t color);
    void set_palette(uint8_t palette_index, uint32_t color);

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
    char _canon_buffer_prev[CANONICAL_BUFFER_SIZE+1];
    EscapeState _canon_escape_state = EscapeState::NORMAL;
    uint16_t _canon_index = 0;
    void _canon_on_key(char c);
    void _canon_send();

    // Character buffer
    uint16_t _char_buffer[COLUMNS * ROWS];
    bool _dirty = false;    // Has the character buffer been modified since the last update?

    // Escape sequence handling
    EscapeState _escape_state = EscapeState::NORMAL;
    char _escape_params[32];
    char _escape_params_index = 0;

    // Alternate buffer
    struct TerminalState *_prev_state;

    // Terminal attributes
    uint8_t _current_attributes;    // The current character attributes!
    uint8_t _color_default_fg = ANSIColors::ANSI_COLOR_WHITE;   // Default foreground text color
    uint8_t _color_default_bg = ANSIColors::ANSI_COLOR_BLACK;   // Default background text color
    // 7 - Inverted
    // 6 - Bold
    // 5-3 - FG color index
    // 2-0 - BG color index
    uint16_t _palette[16] = {
        TERM_COLOR_BLACK,
        TERM_COLOR_RED,
        TERM_COLOR_GREEN,
        TERM_COLOR_YELLOW,
        TERM_COLOR_BLUE,
        TERM_COLOR_MAGENTA,
        TERM_COLOR_CYAN,
        TERM_COLOR_WHITE,
        // "Bold" colors
        TERM_COLOR_BRIGHT_BLACK,
        TERM_COLOR_BRIGHT_RED,
        TERM_COLOR_BRIGHT_GREEN,
        TERM_COLOR_BRIGHT_YELLOW,
        TERM_COLOR_BRIGHT_BLUE,
        TERM_COLOR_BRIGHT_MAGENTA,
        TERM_COLOR_BRIGHT_CYAN,
        TERM_COLOR_BRIGHT_WHITE
    };

    OnKeyboardCallback _on_keyboard = nullptr;

    void _render_terminal();
    void _handle_cursor();  // Handle newline wrapping and scrolling the character buffer
    void _kb_print(const char *s);    // Print a null-terminated string to the keyboard
    void _escape_sequence_cwrite(const char c);
    void _dispatch_escape_sequence(const char *params, char c);
    void _send_cursor_position_response();
    void _save_terminal_state();
    void _restore_terminal_state();
    void _escape_color_graphic_handler(const int *args, const int arg_count);
};