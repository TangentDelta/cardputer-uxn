#pragma once
#include <WiFi.h>
#include <M5Cardputer.h>
#include <functional>

#define CONNECTION_RETRY 5000
#define CONNECTION_TIMEOUT 5000

class WiFiHandler
{
public:
    using FailCallback = std::function<void()>;
    using SuccessCallback = std::function<void()>;

    bool begin();
    void update();
    void set_ssid(const char *ssid, const char *password="");
    void set_password(const char *password);
    void on_connect_fail(FailCallback cb){ _fail_callback = cb; }
    void on_connect_success(SuccessCallback cb){ _success_callback = cb; }
    uint8_t get_status();
    
private:
    enum class WiFiHandlerState
    {
        IDLE,
        DISCONNECTED,
        CONNECTING,
        CONNECTED
    };
    
    unsigned long _connect_start_ms = 0;
    WiFiHandlerState _state = WiFiHandlerState::IDLE;
    char _ssid[32];
    char _password[32];

    FailCallback _fail_callback;
    SuccessCallback _success_callback;
};