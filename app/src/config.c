#include <stdio.h>
#include <api_fs.h>
#include <api_info.h>

#include "config.h"
#include "config_parser.h"
#include "gps_tracker.h"

Config g_ConfigStore;

void FsInfoTest()
{
    API_FS_INFO fsInfo;
    int sizeUsed = 0, sizeTotal = 0;

    UART_Log("Start Fs info test!\r\n");
    
    if(API_FS_GetFSInfo(FS_DEVICE_NAME_FLASH, &fsInfo) < 0)
    {
        UART_Log("Get FS Flash device info fail!\r\n");
    }
    sizeUsed  = fsInfo.usedSize;
    sizeTotal = fsInfo.totalSize;
    UART_Log("flash used:%d Bytes, total size:%d Bytes",sizeUsed,sizeTotal);
    if(API_FS_GetFSInfo(FS_DEVICE_NAME_T_FLASH, &fsInfo) < 0)
    {
        UART_Log("Get FS T Flash device info fail!\r\n");
    }
    sizeUsed  = fsInfo.usedSize;
    sizeTotal = fsInfo.totalSize;
    float mb = sizeTotal/1024.0/1024.0;
    UART_Log("T Flash used:%d Bytes, total size:%d Bytes(%d.%03d MB)\r\n",sizeUsed,sizeTotal,(int)mb, (int)((mb-(int)mb)*1000)  );
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

    if (!Config_Load(&g_ConfigStore, CONFIG_FILE_PATH))
    {
        UART_Log("ERROR - load config file\r\n");
    }

    char *device_name = Config_GetValue(&g_ConfigStore, KEY_DEVICE_NAME, NULL, 0);
    if ((device_name == NULL) || (device_name[0]='\0'))
    {
        char IMEI[16];
        memset(IMEI, 0, sizeof(IMEI));
        if(INFO_GetIMEI(IMEI))
        {
            Config_SetValue(&g_ConfigStore, KEY_DEVICE_NAME, IMEI);
        } else {
            Config_SetValue(&g_ConfigStore, KEY_DEVICE_NAME, DEFAULT_DEVICE_NAME);
        }
    }
}

void HandleHelpCommand(void)
{
    UART_Log("Available commands:\r\n");
    UART_Log("  config               - Print current configuration\r\n");
    UART_Log("  set server <value>   - Set tracking server address\r\n");
    UART_Log("  set port <value>     - Set tracking server port\r\n");
    UART_Log("  set protocol <value> - Set tracking protocol (e.g., osmand, traccar)\r\n");
    UART_Log("  set apn <value>      - Set APN for cellular connection\r\n");
    UART_Log("  set apn_user <value> - Set APN username\r\n");
    UART_Log("  set apn_pass <value> - Set APN password\r\n");
    UART_Log("  help                 - Show this help message\r\n");
}

void HandleSetCommand(char* args)
{
    char buffer[128];
    strncpy(buffer, args, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* field = strtok(buffer, " ");
    char* value = strtok(NULL, "");

    if (!field || !value) {
        UART_Log("Usage: set <field> <value>\r\n");
        return;
    }

    char* key = NULL;

    if (strcmp(field, "server") == 0) {
        key = KEY_TRACKING_SERVER_ADDR;
    } else if (strcmp(field, "port") == 0) {
        key = KEY_TRACKING_SERVER_PORT;
    } else if (strcmp(field, "protocol") == 0) {
        key = KEY_TRACKING_SERVER_PROTOCOL;
    } else if (strcmp(field, "apn") == 0) {
        key = KEY_APN;
    } else if (strcmp(field, "apn_user") == 0) {
        key = KEY_APN_USER;
    } else if (strcmp(field, "apn_pass") == 0) {
        key = KEY_APN_PASS;
    }

    if (key) {
        if (Config_SetValue(&g_ConfigStore, key, value)) {
            UART_Log("Set %s to %s\r\n", field, value);
            if (Config_Save(&g_ConfigStore, CONFIG_FILE_PATH))
            {
                UART_Log("ERROR - save config file\r\n");    
            }
        } else {
            UART_Log("Failed to set %s\r\n", field);
        }
    } else {
        UART_Log("Unknown setting '%s'\r\n", field);
    }
}

void HandleConfigCommand(void)
{
    UART_Log("Printing ConfigStore:\r\n");
    UART_Log("server: %s\r\n",   Config_GetValue(&g_ConfigStore, KEY_TRACKING_SERVER_ADDR,     NULL, 0));
    UART_Log("port: %s\r\n",     Config_GetValue(&g_ConfigStore, KEY_TRACKING_SERVER_PORT,     NULL, 0));
    UART_Log("protocol: %s\r\n", Config_GetValue(&g_ConfigStore, KEY_TRACKING_SERVER_PROTOCOL, NULL, 0));
    UART_Log("apn: %s\r\n",      Config_GetValue(&g_ConfigStore, KEY_APN, NULL, 0));
    UART_Log("apn_user: %s\r\n", Config_GetValue(&g_ConfigStore, KEY_APN_USER, NULL, 0));
    UART_Log("apn_pass: %s\r\n", Config_GetValue(&g_ConfigStore, KEY_APN_PASS, NULL, 0));
}

void HandleUartCommand(char* cmd)
{
    cmd = trim_whitespace(cmd);

    if (strcmp(cmd, "config") == 0)
    {
        HandleConfigCommand();
    }
    else if (strncmp(cmd, "set ", 4) == 0)
    {
        HandleSetCommand(cmd + 4);
    }
    else 
    {
        HandleHelpCommand();
    }
}
