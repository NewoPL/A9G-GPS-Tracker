#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <api_os.h>
#include <api_fs.h>
#include <api_info.h>

#include "debug.h"
#include "gps_tracker.h"
#include "config_validation.h"
#include "config_commands.h"
#include "config_store.h"

t_Config g_ConfigStore;

char* trim_whitespace(char* str)
{
    if (!str) return NULL;

    // trim leading whitespace
    while (isspace((unsigned char)*str)) str++;

    // if there were only whitespaces
    if (*str == '\0') return str;

    // trim trailing whitespace
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    end[1] = '\0';
    return str;
}

static bool parse_line(char* line)
{
    if (!line) return false;
    char* trimmed = trim_whitespace(line);
    if (trimmed[0] == '\0' || trimmed[0] == '#')
        return false;
    char* equals = strchr(trimmed, '=');
    if (!equals)
        return false;
    *equals = '\0';
    t_config_map* entry = getConfigMap(trimmed);
    if (!entry) {
        LOGE("Unknown config key: %s", trimmed);
        return false;
    }
    if (!entry->validator(equals + 1)) {
        LOGE("Invalid value for %s: %s", trimmed, equals + 1);
        return false;
    }
    // Set value logic would go here
    return true;
}

bool ConfigStore_Load(char* filename)
{
    if (!filename) return false;

    int32_t fd = API_FS_Open(filename, FS_O_RDWR | FS_O_CREAT, 0);
    if (fd < 0)
    {
        LOGE("Open file failed: %d", fd);
        return false;
    }

    char buffer[MAX_LINE_LENGTH];
    int leftover = 0; // number of characters in the buffer remained to be processed 
    bool skip_next_line = false; // Flag to skip the next line if it doesn't fit into the buffer

    while (true)
    {
        int32_t read_bytes = API_FS_Read(fd, buffer + leftover, sizeof(buffer) - leftover - 1);
        if (read_bytes < 0)
        {
            LOGE("Read error: %d", read_bytes);
            API_FS_Close(fd);
            return false;
        }

        leftover += read_bytes;
        if (leftover <= 0) break; // EOF

        char* curr_position = buffer;  // pointer to the position in the buffer to start parsing from
        buffer[leftover] = '\0';  // Null terminate the buffer to ensure it's a valid string

        // Find lines using strchr instead of strtok
        while (curr_position < buffer + leftover)
        {
            char* line_end = strchr(curr_position, '\n'); // Find the next newline character

            // If we reach the end of the file and there's no newline, process the remaining buffer
            if ((line_end == NULL) && API_FS_IsEndOfFile(fd))
            {
                line_end = buffer + leftover;
            } 

            if (line_end != NULL)
            {
                if (skip_next_line)
                {
                    LOGD("Skipping long line exceeding buffer size");                    
                    skip_next_line = false; // Reset the flag
                } 
                else
                {
                    *line_end = '\0';  // Null terminate the line
                    // Try to parse the line into a key-value pair
                    parse_line(curr_position);
                }
                curr_position = line_end + 1; // Move past the newline character
            } else {
                if (curr_position == buffer)
                {
                    skip_next_line = true; // Skip the entire line if it does not fit into the buffer
                    curr_position = buffer + leftover; // Move processing past the current buffer
                }
                break;  // No more newlines, break out of the loop
            }
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

        size_t len = snprintf(line_to_save, sizeof(line_to_save)-1, "%-20s = %s\r\n", 
                 g_config_map[i].param_name, value_str);
        line_to_save[len] = '\0'; // Ensure null termination

        if (API_FS_Write(fd, (char*)value_str, len) != len) {
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
        LOGE("Failed to get imei, using default device name: %s", DEFAULT_DEVICE_NAME);

    // For each config entry, validate and set default value
    for (size_t index = 0; index < g_config_map_size; ++index) {
        entry = (t_config_map*)&g_config_map[index];
        if (!entry || !entry->validator) {
            LOGE("No validator for config map entry: %s", entry ? entry->param_name : "(null)");
            continue; // Skip if no validator is defined
        }
        if (!entry->validator(entry->default_value)) {
            LOGE("Invalid default value for %s: %s", entry->param_name, entry->default_value);
            continue; // Skip if default value is invalid
        }
    }

    if (!ConfigStore_Load(CONFIG_FILE_PATH))
    {
        LOGE("load config file failed");
    }
}
