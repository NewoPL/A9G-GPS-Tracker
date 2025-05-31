#ifndef DEBUG_H
#define DEBUG_H

#define DEFAULT_LOG_LEVEL     LOG_LEVEL_INFO
#define DEFAULT_LOG_OUTPUT    LOGGER_OUTPUT_UART
#define LOG_LEVEL_BUFFER_SIZE 1024

// Log levels
typedef enum {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG
} LogLevel;

// Log output
typedef enum {
    LOGGER_OUTPUT_UART = 0,
    LOGGER_OUTPUT_TRACE,
    LOGGER_OUTPUT_FILE
} LoggerOutput;

extern LoggerOutput g_log_output;
extern LogLevel     g_log_level;
extern int32_t      g_log_file;

void     set_log_level(LogLevel level);
char*    log_level_to_string(LogLevel level);
LogLevel log_level_to_int(const char* str);
void     UART_Printf(const char* fmt, ...) ;

void log_message_internal(LogLevel level, const char *func, const char *format, ...);
#define LOG(level, format, ...) \
    log_message_internal(level, __func__, format, ##__VA_ARGS__)
#define LOGE(format, ...) \
    log_message_internal(LOG_LEVEL_ERROR, __func__, format, ##__VA_ARGS__)
#define LOGW(format, ...) \
    log_message_internal(LOG_LEVEL_WARN,  __func__, format, ##__VA_ARGS__)
#define LOGI(format, ...) \
    log_message_internal(LOG_LEVEL_INFO,  __func__, format, ##__VA_ARGS__)
#define LOGD(format, ...) \
    log_message_internal(LOG_LEVEL_DEBUG, __func__, format, ##__VA_ARGS__)

#endif // DEBUG
