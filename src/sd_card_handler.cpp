#include "sd_card_handler.h"

void SDCardHandler::begin()
{
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    SD.begin(SD_SPI_CS_PIN, SPI);
}

File SDCardHandler::open(const char *path, const char *mode)
{
    File f = SD.open(_build_path(path), mode, mode[0] == 'w');
    // Restore the working dir if it was modified
    if(_path_separator != 0)
    {
        working_dir[_path_separator] = '\0';
        _path_separator = 0;
    }
    return f;
}

bool SDCardHandler::exists(const char *path)
{
    bool b = SD.exists(_build_path(path));
    // Restore the working dir if it was modified
    if(_path_separator != 0)
    {
        working_dir[_path_separator] = '\0';
        _path_separator = 0;
    }
    return b;
}

const char* SDCardHandler::_build_path(const char* path)
{
    if(path[0] == '/')  // Absolute path?
    {
        return path;
    }
    else    // If not absolute, it's a realtive path
    {
        _path_separator = strlen(working_dir);  // Save the end of the working path
        strcat(working_dir, path);  // Concat the relative path onto the end
        return working_dir;
    }
}