#include <stdio.h>
#include <api_fs.h>
#include <api_info.h>

#include "debug.h"
#include "config.h"
#include "config_parser.h"
#include "gps_tracker.h"

Config g_ConfigStore;

void FsInfoTest()
{
    API_FS_INFO fsInfo;
    int sizeUsed = 0, sizeTotal = 0;

    LOGI("Start Fs info test!");
    
    if(API_FS_GetFSInfo(FS_DEVICE_NAME_FLASH, &fsInfo) < 0)
    {
        LOGE("Get FS Flash device info fail!");
    }
    sizeUsed  = fsInfo.usedSize;
    sizeTotal = fsInfo.totalSize;
    LOGI("flash used:%d Bytes, total size:%d Bytes",sizeUsed,sizeTotal);
    if(API_FS_GetFSInfo(FS_DEVICE_NAME_T_FLASH, &fsInfo) < 0)
    {
        LOGE("Get FS T Flash device info fail!");
    }
    sizeUsed  = fsInfo.usedSize;
    sizeTotal = fsInfo.totalSize;
    float mb = sizeTotal/1024.0/1024.0;
    LOGI("T Flash used:%d Bytes, total size:%d Bytes(%d.%03d MB)",sizeUsed,sizeTotal,(int)mb, (int)((mb-(int)mb)*1000)  );
}

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

void HandleHelpCommand(void)
{
    UART_Printf("Available commands:\r\n");
    UART_Printf("  help                   - Show this help message\r\n");
    UART_Printf("  show config            - Print all configuration\r\n");
    UART_Printf("  set <param> <value>    - Set value to a specied parameter\r\n");
    UART_Printf("  get <param>            - Print a value of a specified parameter\r\n\r\n");
    UART_Printf("  parameters are:\r\n");
    UART_Printf("     address, port, protocol, apn, apn_user, apn_pass, log_level, log_output, device_name\r\n");
}

static char* get_config_key_by_param(const char* param)
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

void HandleSetCommand(char* args)
{
    char buffer[128];
    strncpy(buffer, args, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* field = strtok(buffer, " ");
    char* value = strtok(NULL, "");

    if (!field || !value) {
        UART_Printf("ERROR - set wrong parameters\r\n");
        return;
    }

    char* key = get_config_key_by_param(field);

    if (key) {
        if (Config_SetValue(&g_ConfigStore, key, value)) {
            LOGI("Set %s to %s", field, value);
            if (!Config_Save(&g_ConfigStore, CONFIG_FILE_PATH))
                LOGE("Failed to save config file");    
        } else {
            LOGE("Failed to set %s", field);
        }
    } else {
        UART_Printf("ERROR - unknown parameter %s\r\n", field);
    }
}

void HandleGetCommand(char* args)
{
    char buffer[128];
    strncpy(buffer, args, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    char* param = trim_whitespace(buffer);
    const char* key = get_config_key_by_param(param);

    if (key) {
        const char* value = Config_GetValue(&g_ConfigStore, key, NULL, 0);
        if (value) {
            UART_Printf("%s: %s\r\n", param, value);
        } else {
            UART_Printf("parameter %s is not configured.\r\n", param);
        }
    } else {
        UART_Printf("ERROR - unknown parameter %s\r\n", param);
        return;
    }
}

void HandleConfigCommand(void)
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


void HandleUartCommand(char* cmd)
{
    cmd = trim_whitespace(cmd);

    if (strcmp(cmd, "config") == 0) {
        HandleConfigCommand();
    } else if (strncmp(cmd, "set ", 4) == 0) {
        HandleSetCommand(cmd + 4);
    } else if (strncmp(cmd, "get ", 4) == 0) {
        HandleGetCommand(cmd + 4);
    } else {
        HandleHelpCommand();
    }
}
