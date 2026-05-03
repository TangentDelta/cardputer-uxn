#include <Arduino.h>
#include <WiFi.h>
#include <sd_card_handler.h>
#pragma once

// Callback
using UxnDeviceCallback = std::function<void(uint8_t)>;

class Uxn
{
public:
    enum ConsoleType
    {
        type_no_queue,
        type_stdin,
        type_argument,
        type_argument_spacer,
        type_argument_end
    };

    enum DevicePorts
    {
        DEVICE_SYSTEM_WST = 0x04,
        DEVICE_SYSTEM_RST = 0x05,
        DEVICE_SYSTEM_METADATA_HI = 0x06,
        DEVICE_SYSTEM_MDETADATA_LO = 0x07,
        DEVICE_SYSTEM_STATE = 0x0f,

        DEVICE_CONSOLE_VECTOR_HI = 0x10,
        DEVICE_CONSOLE_VECTOR_LO = 0x11,
        DEVICE_CONSOLE_READ = 0x12,
        DEVICE_CONSOLE_STTY = 0x14,
        DEVICE_CONSOLE_TYPE = 0x17,
        DEVICE_CONSOLE_WRITE = 0x18,
        DEVICE_CONSOLE_ERROR = 0x19,
    };

    Uxn(uint8_t memory_size=8, uint8_t stack_size=4);
    ~Uxn();
    bool begin();
    void update();
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
    bool console_vector_set = false;
    void console_vector(uint8_t value, ConsoleType value_type = ConsoleType::type_stdin);
    void console_stdin(uint8_t value){ console_vector(value); };

    /* Socket Device */
    bool wifi_connected = false;

    /* File Device */
    SDCardHandler sd_card_handler;

    bool alive = false; // The Uxn instance defaults to being dead. Setting a vector sets this to true.
private:
    enum class FileHandleState
    {
        CLOSED,
        OPEN_READ,
        OPEN_WRITE,
        SOCKET_TCP
    };
    enum FileDevicePorts
    {
        VECTOR_HI,
        VECTOR_LO,
        SUCCESS_HI,
        SUCCESS_LO,
        STAT_HI,
        STAT_LO,
        DELETE,
        APPEND,
        NAME_HI,
        NAME_LO,
        LENGTH_HI,
        LENGTH_LO,
        READ_HI,
        READ_LO,
        WRITE_HI,
        WRITE_LO
    };

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
    UxnDeviceCallback _console_stty = nullptr;

    /* File Device */
    uint16_t _file_ptr;
    File _file_handle[2];
    NetworkClient *_file_socket[2] = {nullptr};
    FileHandleState _file_handle_state[2] = {FileHandleState::CLOSED};
    char _working_file_stat[64] = {0};    // Used to hold the file stat line in case it gets cut off in the middle
    const char *_get_filename(uint8_t *device);    // Get the filename from File/name*
    void _file_close(uint8_t file_index);
    void _file_name(uint8_t *device, uint8_t file_index);
    void _file_read(uint8_t *device, uint8_t file_index);
    void _file_write(uint8_t *device, uint8_t file_index);
    void _file_stat(uint8_t *device, uint8_t file_index);
    void _file_delete(uint8_t *device, uint8_t file_index);
    void _file_dir_content(uint8_t *device, uint8_t file_index);
    // Socket methods
    void _file_socket_connect(uint8_t *device, uint8_t file_index, const char *authority);
    void _file_socket_read(uint8_t *device, uint8_t file_index);
    void _file_socket_write(uint8_t *device, uint8_t file_index);
};