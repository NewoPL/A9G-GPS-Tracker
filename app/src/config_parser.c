#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "api_os.h"
#include "api_fs.h"
#include "api_charset.h"
#include "api_debug.h"

#include "config.h"
#include "config_parser.h"

static char* trim_whitespace(char* str)
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

static bool parse_line(Config* config, char* line)
{
    if (!line) return false;

    // Trim whitespace
    char* trimmed = trim_whitespace(line);

    // Skip empty lines or comments
    if (trimmed[0] == '\0' || trimmed[0] == '#')
        return false;

    // Find '=' separator
    char* equals = strchr(trimmed, '=');
    if (!equals)
        return false;

    // Split key and value
    *equals = '\0';
    
    // Save key and value in stored config
    return Config_SetValue(config, trimmed, equals + 1);
}

bool Config_Load(Config* config, char* filename)
{
    if (!config || !filename)
        return false;

    int32_t fd = API_FS_Open(filename, FS_O_RDWR, 0);
    if (fd < 0)
    {
        Trace(1, "Open file failed: %d", fd);
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
            Trace(1, "Read error: %d", read_bytes);
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
                    Trace(2, "Skipping long line exceeding buffer size");                    
                    skip_next_line = false; // Reset the flag
                } 
                else
                {
                    *line_end = '\0';  // Null terminate the line
                    // Try to parse the line into a key-value pair
                    parse_line(config, curr_position);
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

bool Config_Save(Config* config, char* filename)
{
    if (!config || !filename)
        return false;

    int32_t fd = API_FS_Open(filename, FS_O_RDWR | FS_O_CREAT | FS_O_TRUNC, 0);
    if ( fd < 0)
    {
        Trace(1,"Open file failed:%d",fd);
	    return false;
    }

    for (int i = 0; i < config->count; ++i)
    {
        char line[MAX_LINE_LENGTH];

        int len = snprintf(line, sizeof(line), "%s=%s\n",
                           config->entries[i].key,
                           config->entries[i].value);

        if (len < 0 || len >= sizeof(line)) {
            Trace(1, "Line too long to write (entry %d)", i);
            API_FS_Close(fd);
            return false;
        }

        if (API_FS_Write(fd, line, len) != len) {
            Trace(1, "Write failed at entry %d", i);
            API_FS_Close(fd);
            return false;
        }        
    }

    API_FS_Flush(fd);
    API_FS_Close(fd);
    return true;
}

char* Config_GetValue(Config* config, const char* key, char* out_buffer, size_t buffer_len)
{
    if (!config || !key)
        return NULL;

    for (int i = 0; i < config->count; ++i)
    {
        if (strcmp(config->entries[i].key, key) == 0)
        {
            if (out_buffer && buffer_len > 0)
            {
                // Copy value safely to out_buffer
                strncpy(out_buffer, config->entries[i].value, buffer_len - 1);
                out_buffer[buffer_len - 1] = '\0'; // Ensure null-termination
                return out_buffer;
            }
            else
            {
                // Return internal pointer if no buffer provided
                return config->entries[i].value;
            }
        }
    }

    return NULL;
}

bool Config_SetValue(Config* config, char* key, char* value) 
{
    if (!config || !key || !value)
        return false;

    key = trim_whitespace(key);
    value = trim_whitespace(value);

    // Validate lengths
    size_t key_len = strlen(key);
    size_t value_len = strlen(value);
    
    // Reject if key is empty or too long
    if (key_len == 0 || key_len >= MAX_KEY_LENGTH - 1)
        return false;

    // Reject if value is too long
    if (value_len >= MAX_VALUE_LENGTH - 1)
        return false;

    for (int i = 0; i < config->count; ++i)
    {
        if (strcmp(config->entries[i].key, key) == 0) {
            strncpy(config->entries[i].value, value, MAX_VALUE_LENGTH);
            return true;
        }
    }

    if (config->count >= MAX_CONFIG_LINES)
        return false;

    strncpy(config->entries[config->count].key, key, MAX_KEY_LENGTH);
    strncpy(config->entries[config->count].value, value, MAX_VALUE_LENGTH);
    config->count++;

    return true;
}

bool Config_RemoveKey(Config* config, char* key)
{
    if (!config || !key)
        return false;

    for (int i = 0; i < config->count; ++i)
 {
        if (strcmp(config->entries[i].key, key) == 0)
        {
            for (int j = i; j < config->count - 1; ++j) {
                config->entries[j] = config->entries[j + 1];
            }
            config->count--;
            return true;
        }
    }
    return false;
}

void Config_Purge(Config* config)
{
    config->count = 0;
};
