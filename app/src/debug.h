#ifndef DEBUG_H
#define DEBUG_H

#define DEFAULT_LOG_LEVEL     LOG_LEVEL_INFO
#define LOG_LEVEL_BUFFER_SIZE 1024

// Log levels
typedef enum {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG
} LogLevel;

extern LogLevel g_current_log_level;

void set_log_level(LogLevel level);
void log_message_internal(LogLevel level, const char *func, const char *format, ...);
void UART_Log(const char* fmt, ...);

#define log_message(level, format, ...) \
    log_message_internal(level, __func__, format, ##__VA_ARGS__)

#endif // DEBUG
_H