#include "sd_card_handler.h"

void SDCardHandler::begin()
{
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    SD.begin(SD_SPI_CS_PIN, SPI);
}

File SDCardHandler::open(const char *path, const char *mode)
{
    return SD.open(path, mode, mode[0] == 'w');
}

bool SDCardHandler::exists(const char *path)
{
    return SD.exists(path);
}