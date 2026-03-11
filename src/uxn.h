#include <Arduino.h>
#pragma once

// Callback
using UxnDeviceCallback = std::function<void(uint8_t)>;

class Uxn
{
public:
    unsigned int eval(uint16_t pc);
    void load(const uint8_t *rom, int count);
    void set_deo_callback(uint8_t port, UxnDeviceCallback port_callback);
protected:
    /* Core */
    // Core variables
    uint8_t _ram[0x10000];
    uint8_t _stk[2][0x100];
    uint8_t _ptr[2];

    // Core methods
    uint8_t _dei(const uint8_t port);
    void _deo(const uint8_t port, const uint8_t value);

    /* Console Device */
    uint16_t _cons_vector = 0;
    UxnDeviceCallback _cons_write = nullptr;
};