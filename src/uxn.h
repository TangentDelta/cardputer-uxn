#include <Arduino.h>
#pragma once

// Callback
using UxnDeviceCallback = std::function<void(uint8_t)>;

class Uxn
{
public:
    Uxn(const int ram_size=0x10000, const int stack_size=0x100);
    ~Uxn();
    unsigned int eval(uint16_t pc);
    void load(const uint8_t *rom, int count);
    void set_deo_callback(uint8_t port, UxnDeviceCallback port_callback);

    // Direct memory access
    void mem_poke(uint16_t addr, uint8_t value);
    uint8_t mem_peek(uint16_t addr);
protected:
    /* Core */
    // Core sizing
    unsigned int _ram_size;
    unsigned int _stack_size;
    uint16_t _ram_mask;
    uint8_t _stack_mask;
    // Core variables
    uint8_t _devices[0x100];
    uint8_t *_ram;
    uint8_t *_stk[2];
    uint8_t _ptr[2];

    // Core methods
    uint8_t _dei(const uint8_t port);
    void _deo(const uint8_t port, const uint8_t value);

    /* Console Device */
    uint16_t _cons_vector = 0;
    UxnDeviceCallback _cons_write = nullptr;
};