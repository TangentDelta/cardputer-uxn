#pragma once
#include <M5Cardputer.h>
#include <SPI.h>
#include <SD.h>

#define SD_SPI_SCK_PIN 40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN 12

class SDCardHandler
{
public:
    void begin();
    File open(const char *path, const char *mode);
    bool exists(const char *path);
    bool is_dir(const char *path);
    bool change_dir(const char *path);
    bool mkdir(const char *path);
    bool create_dirs(const char *path);
    bool remove(const char *path);
    char working_dir[128] = "/";
    bool okay = false;
protected:
    uint8_t _path_separator = 0;
    const char* _build_path(const char *path);
    void _restore_path();
};