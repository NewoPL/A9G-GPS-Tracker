#include <stdio.h>
#include <stdarg.h>

#include <api_hal_uart.h>

#include "debug.h"

LogLevel g_current_log_level = DEFAULT_LOG_LEVEL;

static const char* log_level_to_string(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        default:              return "UNKNOWN";
    }
}

void set_log_level(LogLevel level) {
    g_current_log_level = level;
}

void UART_Log(const char* fmt, ...) 
{
    char buffer[LOG_LEVEL_BUFFER_SIZE];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (len > 0) {
        // Clamp to max buffer size
        if ((size_t)len >= sizeof(buffer)) {
            len = sizeof(buffer) - 1;  
        }
        UART_Write(UART1, buffer, (size_t)len);
    }
    
    return;
}

void log_message_internal(LogLevel level, const char *func, const char *format, ...)
{
    if (level > g_current_log_level || level == LOG_LEVEL_NONE)
        return;

    char message[512];
    va_list args;

    // Format the main message
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    // Send the full log line to UART
    UART_Log("[%s] %s(): %s\n", log_level_to_string(level), func, message);
}
