#include "sd_card_handler.h"

void SDCardHandler::begin()
{
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    SD.begin(SD_SPI_CS_PIN, SPI);
}

File SDCardHandler::open(const char *path, const char *mode)
{
    File f = SD.open(_build_path(path), mode, (mode[0] == 'w') || (mode[0] == 'a'));
    // Restore the working dir if it was modified
    _restore_path();
    return f;
}

bool SDCardHandler::exists(const char *path)
{
    bool b = SD.exists(_build_path(path));
    // Restore the working dir if it was modified
    _restore_path();
    return b;
}

bool SDCardHandler::is_dir(const char *path)
{
    const char *full_path = _build_path(path);
    if(!SD.exists(full_path))
    {
        _restore_path();
        return false;
    }

    File f = open(full_path, "r");
    bool b = f.isDirectory();
    f.close();

    _restore_path();

    return b;
}

bool SDCardHandler::change_dir(const char *path)
{
    // Is this new path a valid directory?
    if(!is_dir(path))
        return false;

    // Absolute path?
    if(path[0] == '/')
    {
        // Just copy the new path over
        strcpy(working_dir, path);
        return true;
    }

    // Relative path needs to be appended onto the current working directory
    strcat(working_dir, path);
    return true;
}

bool SDCardHandler::mkdir(const char *path)
{
    bool success = SD.mkdir(_build_path(path));
    _restore_path();

    return success;
}

bool SDCardHandler::create_dirs(const char *path)
{
    char path_buf[128] = {0};   // A place to hold the path as we build it directory-by-directory
    char *p = path_buf;

    // Is the path relative?
    if(path[0] != '/')
    {
        // If so, we need to copy the working directory into the path buffer
        strcpy(path_buf, working_dir);
        // and move the path buffer pointer up
        p+=(strlen(working_dir));
    }
    
    // Create the directories along the path
    while(*path != '\0')
    {
        *(p++) = *path; // Copy the next char into the buffer
        if(*(path++)=='/')
        {
            // Next directory. Check if it exists and create it if it does not
            if(!exists(path_buf))
            {
                if(!mkdir(path_buf))
                    return false;
            }
        }
    }

    return true;
}

bool SDCardHandler::remove(const char* path)
{
    const char *full_path = _build_path(path);

    // Check if the path is a directory
    File f = SD.open(full_path);
    if(f.isDirectory())
    {
        // Check if the directory is empty
        int file_count = 0;
        while(f.openNextFile()) file_count++;
        f.close();
        if(file_count > 0)
        {
            _restore_path();
            return false;
        }

        // If the directory is empty, remove it and return success
        SD.rmdir(full_path);
        _restore_path();
        return true;
    }
    f.close();  // Don't need the file open any more

    // The path isn't a directory
    bool b = SD.remove(full_path);   // Delete the file
    _restore_path();    // Restore the working directory
    return b;
}

const char* SDCardHandler::_build_path(const char* path)
{
    if(path[0] == '/')  // Absolute path?
    {
        return path;
    }
    else if(path[0] == '.') // Working directory?
    {
        // TODO: Handle relative to wirking directory cases like "./foo_bar"
        return working_dir; // Just return the working directory
    }
    else    // If not absolute, it's a realtive path
    {
        _path_separator = strlen(working_dir);  // Save the end of the working path
        strcat(working_dir, path);  // Concat the relative path onto the end
        return working_dir;
    }
}

void SDCardHandler::_restore_path()
{
    if(_path_separator != 0)
    {
        working_dir[_path_separator] = '\0';
        _path_separator = 0;
    }
}