#include <api_fs.h>
#include <api_hal_uart.h>

#include "utils.h"
#include "config_store.h"
#include "config_validation.h"
#include "debug.h"

int32_t g_log_file;

int32_t UART_Printf(const char* fmt, ...) 
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
    return 0;
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

void log_message_internal(t_logLevel level, const char *tag, const char *format, ...)
{
    if (level > g_ConfigStore.logLevel || level == LOG_LEVEL_NONE)
        return;

    char message[LOG_LEVEL_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    message[sizeof(message) - 1] = '\0';
    va_end(args);

    RTC_Time_t time;
    TIME_GetRtcTime(&time);

    char timebuf[16];
    snprintf(timebuf, sizeof(timebuf), "%02u:%02u.%02u", time.hour,time.minute,time.second);

    // Get logger output type from ConfigStore
    switch (g_ConfigStore.logOutput) {
        case LOGGER_OUTPUT_TRACE:
            Trace(level, "[%s] [%s] %s\n", timebuf, tag, message);
            break;
        case LOGGER_OUTPUT_FILE:
            FILE_Printf("[%s] [%s] [%s] %s\n", timebuf, tag, LogLevelSerializer(&level), message);
            break;
        default:
            UART_Printf("[%s] [%s] [%s] %s\n", timebuf, tag, LogLevelSerializer(&level), message);
            break;
    }
}
