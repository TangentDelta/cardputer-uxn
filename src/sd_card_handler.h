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
protected:
};