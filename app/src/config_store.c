#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <api_os.h>
#include <api_fs.h>
#include <api_info.h>

#include "gps_tracker.h"
#include "config_store.h"
#include "config_commands.h"
#include "config_validation.h"
#include "minmea.h"
#include "utils.h"
#include "debug.h"

t_Config g_ConfigStore;

static bool parse_line(char* line)
{
    if (!line) return false;

    char* value = strchr(line, '=');
    if (!value) return false;
    *value = '\0';

    line  = trim_whitespace(line);
    value = trim_whitespace(++value);

    t_config_map* entry = getConfigMap(line);
    if (!entry) {
        LOGE("Unknown config key: %s", line);
        return false;
    }

    if (!entry->validator(value)) {
        LOGE("Invalid value for %s: %s", line, value);
        return false;
    }

    return true;
}

bool ConfigStore_Load(char* filename)
{
    if (!filename) return false;

    int32_t fd = API_FS_Open(filename, FS_O_RDWR | FS_O_CREAT, 0);
    if (fd < 0)
    {
        LOGD("Open file %s failed. err= %d", filename, fd);
        return false;
    }

    int file_size = (int)API_FS_GetFileSize(fd);
    if (file_size <=0)
    {
        LOGD("File %s is empty. err= %d", filename, file_size);
        return true;
    }

    char buffer[MAX_LINE_LENGTH];
    // Flag to skip the next line if it doesn't fit into the buffer
    bool skip_next_line = false; 
    // number of characters in the buffer remained to be processed from previous chunk
    int leftover = 0; 

    while (true)
    {
        int32_t read_bytes = API_FS_Read(fd, buffer + leftover, sizeof(buffer) - leftover - 1);
        if (read_bytes < 0)
        {
            LOGD("Read error: %d", read_bytes);
            API_FS_Close(fd);
            return false;
        }
        file_size -= read_bytes; 
        leftover += read_bytes;
        
        // break the loop if the end of file is reached. no characters to process in the buffer
        if (leftover <= 0) break;

        char* curr_position = buffer;  // pointer to the position in the buffer to start parsing from
        buffer[leftover] = '\0';       // Null terminate the buffer to ensure it's a valid string

        // Find the next newline character
        char* line_end = strchr(curr_position, '\n'); 
        
        // if there is no new line character in the full buffer set to skip the next line 
        // and read the next part of the file
        if ((line_end == NULL) &&  (leftover >= sizeof(buffer) - 1))
        {
            LOGD("Buffer overflow, skipping long line");
            skip_next_line = true; // Skip the entire line if it does not fit into the buffer
            leftover = 0;          // Move processing past the current buffer
            continue;
        }

        // Find lines using strchr instead of strtok
        while (curr_position < buffer + leftover)
        {
            if ((line_end == NULL) && (file_size <= 0))
            {
                // if we reached here
                // it means we have the last line that does not end with a newline
                // process the remaining buffer
                line_end = buffer + leftover;
            } 

            // No more newlines found, break the loop
            if (line_end == NULL) break;

            if (skip_next_line)
            {
                LOGW("Skipping long line exceeding buffer size");                    
                skip_next_line = false; // Reset the flag
            } 
            else
            {
                *line_end = '\0';  // Null terminate the line
                // Try to parse the line into a key-value pair
                parse_line(curr_position);
            }
            curr_position = line_end + 1; // Move past the newline character
            
            // If we moved past the valid data in the buffer, break the loop
            if (curr_position >= buffer + leftover) break;
            
            // Find the next newline character
            line_end = strchr(curr_position, '\n');
        }

        // If there are leftover characters after processing lines, move them to the beginning of the buffer
        leftover = (int)(buffer + leftover - curr_position);
        if (leftover > 0) {
            memmove(buffer, curr_position, leftover);
        }
    }

    API_FS_Close(fd);  // Close the file after reading
    return true;       // Successfully loaded the config
}

bool ConfigStore_Save(char* filename)
{
    if (!filename) return false;

    int32_t fd = API_FS_Open(filename, FS_O_RDWR | FS_O_CREAT | FS_O_TRUNC, 0);
    if (fd < 0)
    {
        LOGE("Open config file failed: %d", fd);
        return false;
    }

    static char line_to_save[MAX_LINE_LENGTH];

    for (size_t i = 0; i < g_config_map_size; ++i) {
        const char* value_str = g_config_map[i].serializer(g_config_map[i].value);

        if (value_str == NULL) {
            LOGE("Serializer for %s failed.", g_config_map[i].param_name);
            continue;
        }

        int len = snprintf(line_to_save, sizeof(line_to_save)-1, "%-20s = %s\r\n", 
                 g_config_map[i].param_name, value_str);
        line_to_save[len] = '\0'; // Ensure null termination
        
        if (API_FS_Write(fd, line_to_save, len) != len) {
            LOGE("Write failed for %s", g_config_map[i].param_name);
            continue;
        }
    }

    API_FS_Flush(fd);
    API_FS_Close(fd);
    return true;
}

void ConfigStore_Init()
{
    memset(g_ConfigStore.imei, 0, sizeof(g_ConfigStore.imei));
    t_config_map* entry = getConfigMap(PARAM_DEVICE_NAME);
    if(entry && INFO_GetIMEI(g_ConfigStore.imei))
        entry->default_value = g_ConfigStore.imei;
    else
        UART_Printf("Failed to get imei, using default device name: %s", DEFAULT_DEVICE_NAME);

    // For each config entry, validate and set default value
    for (size_t index = 0; index < g_config_map_size; ++index) {
        entry = (t_config_map*)&g_config_map[index];
        if (!entry->validator) {
            UART_Printf("No validator for config map entry: %s\r\n", entry->param_name);
            continue; // Skip if no validator is defined
        }
        if (!entry->validator(entry->default_value)) {
            UART_Printf("Invalid default value for %s: %s\r\n", entry->param_name, entry->default_value);
            continue; // Skip if default value is invalid
        }
    }

    if (!ConfigStore_Load(CONFIG_FILE_PATH))
    {
        LOGE("loading config file failed: %s",CONFIG_FILE_PATH);
    }
}
