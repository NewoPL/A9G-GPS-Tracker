#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include "api_fs.h"
#include <api_hal_uart.h>

#include "debug.h"
#include "config_commands.h"

int32_t      g_log_file;
LogLevel     g_log_level  = DEFAULT_LOG_LEVEL;
LoggerOutput g_log_output = DEFAULT_LOG_OUTPUT;

char* log_level_to_string(LogLevel level)
{
    switch (level) {
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        default:              return "UNKNOWN";
    }
}

LogLevel log_level_to_int(const char* str)
{
    char buf[16];
    size_t i;
    for (i = 0; i < sizeof(buf) - 1 && str[i]; ++i)
        buf[i] = toupper((unsigned char)str[i]);
    buf[i] = '\0';

    if (strcmp(buf, "ERROR") == 0)
        return LOG_LEVEL_ERROR;
    else if (strcmp(buf, "WARN") == 0)
        return LOG_LEVEL_WARN;
    else if (strcmp(buf, "INFO") == 0)
        return LOG_LEVEL_INFO;
    else if (strcmp(buf, "DEBUG") == 0)
        return LOG_LEVEL_DEBUG;
    else
        return LOG_LEVEL_NONE;
}

void set_log_level(LogLevel level) {
    g_log_level = level;
}

void UART_Printf(const char* fmt, ...) 
{
    char buffer[LOG_LEVEL_BUFFER_SIZE];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (len > 0) {
        // Clamp to max buffer size
        if ((size_t)len >= sizeof(buffer))
            len = sizeof(buffer) - 1;  

        UART_Write(UART1, buffer, (size_t)len);
    }
    return;
}

int32_t FILE_Printf(const char* fmt, ...)
{
    char buffer[LOG_LEVEL_BUFFER_SIZE];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (len > 0) {
        // Clamp to max buffer size
        if ((size_t)len >= sizeof(buffer))
            len = sizeof(buffer) - 1;  

        int ret = API_FS_Write(g_log_file, buffer, (size_t)len);
        if (ret <= 0)
            UART_Write(UART1, buffer, (size_t)len);
    }
    return 0;
}

void log_message_internal(LogLevel level, const char *func, const char *format, ...)
{
    if (level > g_log_level || level == LOG_LEVEL_NONE)
        return;

    char message[LOG_LEVEL_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // Get logger output type from ConfigStore
    switch (g_log_output) {
        case LOGGER_OUTPUT_TRACE:
            Trace(level, "[%s] %s(): %s", log_level_to_string(level), func, message);
            break;
        case LOGGER_OUTPUT_FILE:
            FILE_Printf("[%s] %s(): %s\n", log_level_to_string(level), func, message);
            break;
        default:
            UART_Printf("[%s] %s(): %s\n", log_level_to_string(level), func, message);
            break;
    }
}
