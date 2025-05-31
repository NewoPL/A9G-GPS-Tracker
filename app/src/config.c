#include <stdio.h>
#include <stdlib.h>

#include <api_fs.h>
#include <api_info.h>
#include <api_hal_pm.h>

#include "debug.h"
#include "config.h"
#include "config_parser.h"
#include "gps_tracker.h"

typedef void (*UartCmdHandler)(char*);

void HandleHelpCommand(char*);
void HandleConfigCommand(char*);
void HandleLsCommand(char*);
void HandleRemoveFileCommand(char*);
void HandleSetCommand(char*);
void HandleGetCommand(char*);
void HandleTailCommand(char*);
void HandleLogLevelCommand(char*);
void HandleRestartCommand(char*);

struct uart_cmd_entry {
    const char* cmd;
    int cmd_len;
    UartCmdHandler handler;
    const char* syntax;
    const char* help;
};

static struct uart_cmd_entry uart_cmd_table[] = {
    {"help",     4, HandleHelpCommand,       "help",                "Show this help message"},
    {"config",   6, HandleConfigCommand,     "config",              "Print all configuration"},
    {"ls",       2, HandleLsCommand,         "ls [path]",           "List files in specified folder (default: /)"},
    {"rm",       2, HandleRemoveFileCommand, "rm <file>",           "Remove file at specified path"},
    {"set",      3, HandleSetCommand,        "set <param> [value]", "Set value to a specified parameter. if no value provided parameter will be cleared."},
    {"get",      3, HandleGetCommand,        "get <param>",         "Print a value of a specified parameter"},
    {"tail",     4, HandleTailCommand,       "tail <file> [bytes]", "Print last [bytes] of file (default: 500)"},
    {"loglevel", 8, HandleLogLevelCommand,   "loglevel <level>",    "Set log level (error, warn, info, debug)"},
    {"restart",  7, HandleRestartCommand,    "restart",             "Restart the system immediately"},
};

Config g_ConfigStore;

void ConfigStore_Init()
{
    Config_Purge(&g_ConfigStore);
    Config_SetValue(&g_ConfigStore, KEY_APN, DEFAULT_APN_VALUE);
    Config_SetValue(&g_ConfigStore, KEY_APN_USER, DEFAULT_APN_USER_VALUE);
    Config_SetValue(&g_ConfigStore, KEY_APN_PASS, DEFAULT_APN_PASS_VALUE);
    Config_SetValue(&g_ConfigStore, KEY_TRACKING_SERVER_ADDR, DEFAULT_TRACKING_SERVER_ADDR);
    Config_SetValue(&g_ConfigStore, KEY_TRACKING_SERVER_PORT, DEFAULT_TRACKING_SERVER_PORT);
    Config_SetValue(&g_ConfigStore, KEY_TRACKING_SERVER_PROTOCOL, DEFAULT_TRACKING_SERVER_PROTOCOL);
    Config_SetValue(&g_ConfigStore, KEY_LOG_LEVEL, log_level_to_string(LOG_LEVEL_INFO));
    Config_SetValue(&g_ConfigStore, KEY_LOG_OUTPUT, "UART");

    if (!Config_Load(&g_ConfigStore, CONFIG_FILE_PATH))
    {
        LOGE("ERROR - load config file");
    }

    char *device_name = Config_GetValue(&g_ConfigStore, KEY_DEVICE_NAME, NULL, 0);
    if ((device_name == NULL) || (device_name[0]='\0'))
    {
        char IMEI[16];
        memset(IMEI, 0, sizeof(IMEI));
        if(INFO_GetIMEI(IMEI))
            Config_SetValue(&g_ConfigStore, KEY_DEVICE_NAME, IMEI);
        else
            Config_SetValue(&g_ConfigStore, KEY_DEVICE_NAME, DEFAULT_DEVICE_NAME);    
    }
}

static const char* get_config_key_by_param(const char* param)
{
    struct { const char* param; const char* key; } param_map[] = {
        {"server",      KEY_TRACKING_SERVER_ADDR},
        {"port",        KEY_TRACKING_SERVER_PORT},
        {"protocol",    KEY_TRACKING_SERVER_PROTOCOL},
        {"apn",         KEY_APN},
        {"apn_user",    KEY_APN_USER},
        {"apn_pass",    KEY_APN_PASS},
        {"log_level",   KEY_LOG_LEVEL},
        {"log_output",  KEY_LOG_OUTPUT},
        {"device_name", KEY_DEVICE_NAME},
    };
    for (unsigned i = 0; i < sizeof(param_map)/sizeof(param_map[0]); ++i) {
        if (strcmp(param, param_map[i].param) == 0) {
            return param_map[i].key;
        }
    }
    return NULL;
}

void HandleSetCommand(char* param)
{
    param = trim_whitespace(param);
    if (!*param) {
        UART_Printf("missing variable\r\n");
        return;
    }

    // Find the first space (if any)
    char* space = strchr(param, ' ');
    char* field = param;
    char* value = NULL;
    if (space) {
        *space = '\0';
        value = space + 1;
    } else {
        // No value provided, set to empty string
        value = (char*)"";
    }

    const char* key = get_config_key_by_param(field);
    if (!key) {
        UART_Printf("unknown variable\r\n");
        return;
    }

    if (Config_SetValue(&g_ConfigStore, (char*)key, value))
    {
        LOGI("Set %s to '%s'", field, value);
        if (!Config_Save(&g_ConfigStore, CONFIG_FILE_PATH))
            LOGE("Failed to save config file");    
    }
    else
    {
        LOGE("Failed to set %s", field);
    }
    
    return;
}

void HandleGetCommand(char* param)
{
    char* field = trim_whitespace(param);
    if (!field) {
        UART_Printf("missing variable\r\n");
        return;
    }

    const char* key = get_config_key_by_param(param);
    if (!key) {
        UART_Printf("unknown variable\r\n");
        return;
    }

    const char* value = Config_GetValue(&g_ConfigStore, key, NULL, 0);
    if (value) {
        UART_Printf("%s: %s\r\n", param, value);
    } else {
        UART_Printf("variable %s is not set.\r\n", param);
    }
    
    return;
}

void HandleLsCommand(char* path)
{
    path = trim_whitespace(path);
    if (!path || path[0] == '\0') path = "/";

    UART_Printf("File list in %s:\r\n", path);
    Dir_t* dir = API_FS_OpenDir(path);
    if (!dir) {
        UART_Printf("cannot open directory: %s\r\n", path);
        return;
    }

    char file_path[256];
    strncpy(file_path, path, sizeof(file_path) - 1);
    file_path[sizeof(file_path) - 1] = '\0';
    size_t base_path_len = strlen(file_path);
    // Ensure file_path ends with '/'
    if (base_path_len == 0 || file_path[base_path_len - 1] != '/') {
        if (base_path_len < sizeof(file_path) - 1) {
            file_path[base_path_len] = '/';
            file_path[base_path_len + 1] = '\0';
            base_path_len++;
        }
    }

    Dirent_t* dirent = NULL;

    while ((dirent = API_FS_ReadDir(dir)))
    {
        if (dirent->d_type == 8)
        {
            int file_size = 0;
            strncpy(file_path + base_path_len, dirent->d_name, sizeof(file_path) - base_path_len - 1);
            file_path[base_path_len + strlen(dirent->d_name)] = '\0';
            int32_t fd = API_FS_Open(file_path, FS_O_RDONLY, 0);
            if(fd < 0)
                LOGE("open file %s fail", file_path);
            else
            {
                file_size = (int)API_FS_GetFileSize(fd);
                API_FS_Close(fd);
            }
            UART_Printf("<file>  %-*s  %8d\r\n", 20, dirent->d_name, file_size);
        } else if (dirent->d_type == 4) {
            UART_Printf("<dir>   %-*s\r\n", 20, dirent->d_name);
        }
        else 
            UART_Printf("<unknown>\r\n");
    }
    API_FS_CloseDir(dir);
}

void HandleRemoveFileCommand(char* path)
{
    path = trim_whitespace(path);
    if (!path || path[0] == '\0') {
        UART_Printf("no file specified\r\n");
        return;
    }
    int result = API_FS_Delete(path);
    if (result == 0) {
        UART_Printf("File deleted: %s\r\n", path);
    } else {
        UART_Printf("failed to delete file: %s\r\n", path);
    }
}

void HandleTailCommand(char* args)
{
    int bytes = 500;
    char* file_path = trim_whitespace(args);
    if (!file_path || !*file_path) {
        UART_Printf("missing file\r\n");
        return;
    }

    char* space = strchr(file_path, ' ');
    if (space) {
        *space = '\0';
        char* bytes_str = trim_whitespace(space + 1);
        if (bytes_str && *bytes_str) {
            int val = atoi(bytes_str);
            if (val > 0) bytes = val;
        }
    }

    int32_t fd = API_FS_Open(file_path, FS_O_RDONLY, 0);
    if (fd < 0) {
        UART_Printf("tail: cannot open file %s\r\n", file_path);
        return;
    }
    int file_size = (int)API_FS_GetFileSize(fd);
    if (file_size < 0) {
        UART_Printf("tail: cannot get file size\r\n");
        API_FS_Close(fd);
        return;
    }
    int start = file_size > bytes ? file_size - bytes : 0;
    if (API_FS_Seek(fd, start, 0) < 0) {
        UART_Printf("tail: seek error\r\n");
        API_FS_Close(fd);
        return;
    }
    char buf[128];
    int remaining = file_size - start;
    while (remaining > 0) {
        int to_read = remaining > (int)sizeof(buf) ? (int)sizeof(buf) : remaining;
        int n = API_FS_Read(fd, buf, to_read);
        if (n <= 0) break;
        for (int i = 0; i < n; ++i) UART_Printf("%c", buf[i]);
        remaining -= n;
    }
    UART_Printf("\r\n");
    API_FS_Close(fd);
}

void HandleLogLevelCommand(char* param)
{
    param = trim_whitespace(param);
    if (!param || !*param) {
        UART_Printf("missing log level\r\n");
        return;
    }
    LogLevel level = log_level_to_int(param);
    set_log_level(level);
    Config_SetValue(&g_ConfigStore, KEY_LOG_LEVEL, param);
    Config_Save(&g_ConfigStore, CONFIG_FILE_PATH);
    UART_Printf("Log level set to %s\r\n", param);
}

void HandleRestartCommand(char* args)
{
    UART_Printf("System restarting...\r\n");
    PM_Restart();
}

void HandleConfigCommand(char* args)
{
    UART_Printf("Current configuration:\r\n");
    UART_Printf("  server      : %s\r\n", Config_GetValue(&g_ConfigStore, KEY_TRACKING_SERVER_ADDR, NULL, 0));
    UART_Printf("  port        : %s\r\n", Config_GetValue(&g_ConfigStore, KEY_TRACKING_SERVER_PORT, NULL, 0));
    UART_Printf("  protocol    : %s\r\n", Config_GetValue(&g_ConfigStore, KEY_TRACKING_SERVER_PROTOCOL, NULL, 0));
    UART_Printf("  apn         : %s\r\n", Config_GetValue(&g_ConfigStore, KEY_APN, NULL, 0));
    UART_Printf("  apn_user    : %s\r\n", Config_GetValue(&g_ConfigStore, KEY_APN_USER, NULL, 0));
    UART_Printf("  apn_pass    : %s\r\n", Config_GetValue(&g_ConfigStore, KEY_APN_PASS, NULL, 0));
    UART_Printf("  log_level   : %s\r\n", Config_GetValue(&g_ConfigStore, KEY_LOG_LEVEL, NULL, 0));
    UART_Printf("  log_output  : %s\r\n", Config_GetValue(&g_ConfigStore, KEY_LOG_OUTPUT, NULL, 0));
    UART_Printf("  device_name : %s\r\n", Config_GetValue(&g_ConfigStore, KEY_DEVICE_NAME, NULL, 0));
}

void HandleHelpCommand(char* args)
{
    UART_Printf("\r\nAvailable commands:\r\n");
    for (unsigned i = 0; i < sizeof(uart_cmd_table)/sizeof(uart_cmd_table[0]); ++i) {
        UART_Printf("  %-24s- %s\r\n", uart_cmd_table[i].syntax, uart_cmd_table[i].help);
    }
    UART_Printf("\r\n  <param> is one of:\r\n");
    UART_Printf("     address, port, protocol, apn, apn_user, apn_pass, log_level, log_output, device_name\r\n");
}


void HandleUartCommand(char* cmd)
{
    cmd = trim_whitespace(cmd);
    for (unsigned i = 0; i < sizeof(uart_cmd_table)/sizeof(uart_cmd_table[0]); ++i) {
        const char* c = uart_cmd_table[i].cmd;
        int len = uart_cmd_table[i].cmd_len;
        if (strncmp(cmd, c, len) == 0 && (cmd[len] == ' ' || cmd[len] == '\0')) {
            uart_cmd_table[i].handler(cmd + len);
            return;
        }
    }
    UART_Printf("Unknown command. Type 'help' to see available commands.\r\n");
}
