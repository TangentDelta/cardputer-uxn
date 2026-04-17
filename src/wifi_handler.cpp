#include "wifi_handler.h"

bool WiFiHandler::begin()
{
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    _connect_start_ms = 0;
    return true;
}

void WiFiHandler::update()
{
    unsigned long now = millis();
    switch(_state)
    {
        case(WiFiHandlerState::DISCONNECTED):
            if(strlen(_ssid) == 0) return;

            if((now - _connect_start_ms) > CONNECTION_TIMEOUT)
            {
                WiFi.begin(_ssid, _password);
                _state = WiFiHandlerState::CONNECTING;
                _connect_start_ms = now;
            }

            break;
        case(WiFiHandlerState::CONNECTING):
            if(WiFi.status() == WL_CONNECTED)
            {
                if(_success_callback) _success_callback();
                _state = WiFiHandlerState::CONNECTED;
                break;
            }
            else if((now - _connect_start_ms) > CONNECTION_TIMEOUT)
            {
                {
                    WiFi.disconnect(false);
                    if(_fail_callback) _fail_callback();
                    _connect_start_ms = now;
                    _state = WiFiHandlerState::DISCONNECTED;
                }
            }
        case(WiFiHandlerState::CONNECTED):
            break;
    }
}

void WiFiHandler::set_ssid(const char *ssid, const char *password)
{
    _state = WiFiHandlerState::DISCONNECTED;
    strncpy(_ssid, ssid, 31);
    if(strlen(password) > 0) strncpy(_password, password, 31);
}

void WiFiHandler::set_password(const char *password)
{
    _state = WiFiHandlerState::DISCONNECTED;
    strncpy(_password, password, 31); 
}

uint8_t WiFiHandler::get_status()
{
    return WiFi.status();
}