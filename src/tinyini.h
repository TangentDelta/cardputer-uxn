#pragma once
#include <stdint.h>
#include <string.h>
#include <functional>

using KeyCallback = std::function<void(const char *value)>;

enum class TinyINIStatus
{
    OKAY,
    ERROR_OVERFLOW, // A token exceeded the buffer size
    ERROR_SYNTAX    // Unexpected character for the current parser state
};

template <uint8_t MaxSections, uint8_t MaxKeys, uint8_t BufSize = 64>
class TinyINI
{
public:
    TinyINI(){ reset(); }

    // Register a section in the section registry
    // Returns the ID of the new section for use later when registering a key (1-based)
    // Returns 0 if there are no more free section registry slots
    uint8_t register_section(const char *section)
    {
        // Make sure there's room for another section!
        if(_num_sections >= MaxSections) return 0;  // 0 = no more slots in the registry

        uint8_t section_id = _num_sections+1;   // Section IDs are 1-based
        strncpy(_sections[_num_sections], section, BufSize-1);  // Copy the section into the registry
        _sections[_num_sections][BufSize-1] = '\0'; // Null-terminate the section name in case it exceeds the buffer size
        _num_sections++;
        return section_id;
    }

    // Registers a key in the key registry
    // Returns the ID of the new key (1-based)
    // Returns 0 if there are no more free key registry slots
    uint8_t register_key(uint8_t section_index, const char *key, KeyCallback cb)
    {
        // Make sure there's a free slot for a new key
        if(_num_keys >= MaxKeys) return 0;
        // Sanity check the section index
        if((section_index == 0) || (section_index > _num_sections)) return 0;

        uint8_t key_id = _num_keys+1;
        strncpy(_keys[_num_keys].key, key, BufSize-1);
        _keys[_num_keys].key[BufSize-1] = '\0'; // Null-terminate the key just in case it exceeds the buffer size
        _keys[_num_keys].section_id = section_index;
        _keys[_num_keys].cb = cb;
        _num_keys++;
        return key_id;
    }

    // The main parsing method
    // Takes in one character at a time and calls the appropriate key handlers if a match is found
    // Returns a status code
    TinyINIStatus parse(const char c)
    {
        if(c == '\r') return TinyINIStatus::OKAY;    // Ignore carriage-return characters
        if(c == '\n')
        {
            // Newline ends the current value or comment
            if(_state == STATE_VALUE) _trigger_key();
            _state = STATE_LINE_START;
            _buf_pos = 0;
            return TinyINIStatus::OKAY;
        }

        switch(_state)
        {
            case STATE_LINE_START:
                if((c == ' ') || (c == '\t')) break;    // Skip over leading whitespace characters
                if((c == ';') || (c == '#')){ _state = STATE_COMMENT; break; }  // Comments
                if(c == '['){ _state = STATE_SECTION; _buf_pos = 0; break; }    // Start of a section
                if(_is_key_char(c)) // Valid character for a key
                {
                    _state = STATE_KEY;
                    _buf_pos = 0;
                    _buf[_buf_pos++] = c;
                    break;
                }
                // Not a valid character, so return with a syntax error
                return TinyINIStatus::ERROR_SYNTAX;
            case STATE_COMMENT: break;  // Comments are consumed until a newline is encountered
            case STATE_SECTION:
                if(c == ']')    // Closing section bracket
                {
                    _buf[_buf_pos] = '\0';  // Null-terminate the section
                    _find_section();    // Look up the section and set it as active
                    _state = STATE_LINE_START;  // Reset the state
                    _buf_pos = 0;
                }
                else
                {
                    // Make sure we can fit this char in the buffer
                    if(_buf_pos >= (BufSize-1)) return TinyINIStatus::ERROR_OVERFLOW;
                    _buf[_buf_pos++] = c;
                }
                break;
            case STATE_KEY:
                if((c == '=') || (c == ':'))    // Key-value separator
                {
                    // Remove trailing whitespaces from the key
                    while((_buf_pos > 0) && ((_buf[_buf_pos-1] == ' ') || (_buf[_buf_pos-1] == '\t'))) _buf_pos--;
                    _buf[_buf_pos] = '\0';  // Null-terminate the key
                    _find_key();    // Look up the key and set it as active
                    _state = STATE_VALUE;
                    _buf_pos = 0;
                }
                else
                {
                    // Make sure this character will fit in the buffer
                    if(_buf_pos >= (BufSize-1)) return TinyINIStatus::ERROR_OVERFLOW;
                    _buf[_buf_pos++] = c;
                }
                break;
            case STATE_VALUE:
                // Skip past the leading whitespace for the value
                if((_buf_pos == 0) && ((c == ' ') || (c == '\t'))) break;
                if((c == ';') || (c == '#'))    // Inline comment
                {
                    _trigger_key();
                    _state = STATE_COMMENT;
                }
                else
                {
                    // Make sure this character will fit in the buffer
                    if(_buf_pos >= (BufSize-1)) return TinyINIStatus::ERROR_OVERFLOW;
                    _buf[_buf_pos++] = c;
                }
                break;
        }

        return TinyINIStatus::OKAY;
    }

    // Call to wrap up any unfinished lines
    void finish()
    {
        if(_state == STATE_VALUE) _trigger_key();
        _state = STATE_LINE_START;
        _buf_pos = 0;
    }

    // Reset the parser to a good initial state
    void reset()
    {
        _state = STATE_LINE_START;
        _buf_pos = 0;
        _active_section = 0;
        _active_key = 0;
        _buf[0] = '\0';
    }

private:
    enum State
    {
        STATE_LINE_START,
        STATE_COMMENT,
        STATE_SECTION,
        STATE_KEY,
        STATE_VALUE
    };

    struct KeyEntry
    {
        char key[BufSize];
        KeyCallback cb;
        uint8_t section_id;
    };

    // Big chunky member variables
    char _sections[MaxSections][BufSize];   // Section registry
    KeyEntry _keys[MaxKeys];    // Key registry
    char _buf[BufSize]; // Shared scratchpad buffer for tokens

    uint8_t _num_sections = 0;  // How many sections are currently registered
    uint8_t _num_keys = 0;  // How many keys are registered
    uint8_t _buf_pos = 0;
    uint8_t _active_section = 0;    // The current section index, 1-based. 0=None
    uint8_t _active_key = 0;    // The current key. MaxKeys=None

    State _state = STATE_LINE_START;

    // Private method start

    // Checks if c is a valid character for use in a key
    // [a-zA-Z0-9\_\-\.]
    static bool _is_key_char(char c)
    {
        return (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        (c == '_') || (c == '-') || (c == '.');
    }

    // Look for the section in the registry using the contents of the buffer
    void _find_section()
    {
        _active_section = 0;
        for(uint8_t i = 0; i < _num_sections; i++)
        {
            if(strncmp(_buf, _sections[i], BufSize) == 0)
            {
                // Section found, update the active section index
                _active_section = i+1;  // Add 1 since the indexes are 1-based
                return;
            }
        }

        // The section wasn't found in the registry
    }

    // Look for the key in the registry using the contents of the buffer
    void _find_key()
    {
        _active_key = MaxKeys;
        for(uint8_t i = 0; i < _num_keys; i++)
        {
            if(_keys[i].section_id == _active_section)
            {
                // We're in the right section
                if(strncmp(_buf, _keys[i].key, BufSize) == 0)
                {
                    // and the key matches
                    _active_key = i;
                    return;
                }
            }
        }

        // The key wasn't found in the active section
    }

    // Call the callback for the active key
    void _trigger_key()
    {
        // Make sure the key was found, return if it wasn't
        if(_active_key >= MaxKeys) return;

        // Trim off any trailing whitespace
        while((_buf_pos > 0) && ((_buf[_buf_pos-1] == ' ') || (_buf[_buf_pos-1] == '\t')))
            _buf[_buf_pos--] = '\0';

        // Call the key's callback with the buffer
        if(_keys[_active_key].cb) _keys[_active_key].cb(_buf);

        // Reset the active key
        _active_key = MaxKeys;
    }
};