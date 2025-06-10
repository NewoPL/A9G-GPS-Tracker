#ifndef DEBUG_H
#define DEBUG_H

#define LOG_LEVEL_BUFFER_SIZE 1024

extern int32_t      g_log_file;

void UART_Printf(const char* fmt, ...) ;

void log_message_internal(t_logLevel level, const char *func, const char *format, ...);
#define LOG(level, format, ...) \
    log_message_internal(level, MODULE_TAG, format, ##__VA_ARGS__)
#define LOGE(format, ...) \
    log_message_internal(LOG_LEVEL_ERROR, MODULE_TAG, format, ##__VA_ARGS__)
#define LOGW(format, ...) \
    log_message_internal(LOG_LEVEL_WARN,  MODULE_TAG, format, ##__VA_ARGS__)
#define LOGI(format, ...) \
    log_message_internal(LOG_LEVEL_INFO,  MODULE_TAG, format, ##__VA_ARGS__)
#define LOGD(format, ...) \
    log_message_internal(LOG_LEVEL_DEBUG, MODULE_TAG, format, ##__VA_ARGS__)
    
#endif // DEBUG
