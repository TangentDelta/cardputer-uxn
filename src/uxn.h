#include <Arduino.h>
#pragma once

enum ConsoleType
{
    type_no_queue,
    type_stdin,
    type_argument,
    type_argument_spacer,
    type_argument_end
};

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

    // Direct device access
    void dev_poke(uint8_t port, uint8_t value) { _deo(port, value); };
    uint8_t dev_peek(uint8_t port) { return _dei(port); };

    /* Console Device */
    void console_vector(uint8_t value, ConsoleType value_type = ConsoleType::type_stdin);
    void console_stdin(uint8_t value){ console_vector(value); };

    bool alive = false; // The Uxn instance defaults to being dead. Setting a vector sets this to true.
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
    UxnDeviceCallback _console_write = nullptr; // Callback called when the Uxn instance writes to the console device
    UxnDeviceCallback _console_error = nullptr; // Callback called when the Uxn instance writes to the console error device
};