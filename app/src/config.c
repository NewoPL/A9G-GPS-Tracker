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

    LOGI("Start Fs info test!\r\n");
    
    if(API_FS_GetFSInfo(FS_DEVICE_NAME_FLASH, &fsInfo) < 0)
    {
        LOGE("Get FS Flash device info fail!\r\n");
    }
    sizeUsed  = fsInfo.usedSize;
    sizeTotal = fsInfo.totalSize;
    LOGI("flash used:%d Bytes, total size:%d Bytes",sizeUsed,sizeTotal);
    if(API_FS_GetFSInfo(FS_DEVICE_NAME_T_FLASH, &fsInfo) < 0)
    {
        LOGE("Get FS T Flash device info fail!\r\n");
    }
    sizeUsed  = fsInfo.usedSize;
    sizeTotal = fsInfo.totalSize;
    float mb = sizeTotal/1024.0/1024.0;
    LOGI("T Flash used:%d Bytes, total size:%d Bytes(%d.%03d MB)\r\n",sizeUsed,sizeTotal,(int)mb, (int)((mb-(int)mb)*1000)  );
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
        LOGE("ERROR - load config file\r\n");
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
    LOGI("Available commands:\r\n");
    LOGI("  config               - Print current configuration\r\n");
    LOGI("  set server <value>   - Set tracking server address\r\n");
    LOGI("  set port <value>     - Set tracking server port\r\n");
    LOGI("  set protocol <value> - Set tracking protocol (e.g., osmand, traccar)\r\n");
    LOGI("  set apn <value>      - Set APN for cellular connection\r\n");
    LOGI("  set apn_user <value> - Set APN username\r\n");
    LOGI("  set apn_pass <value> - Set APN password\r\n");
    LOGI("  help                 - Show this help message\r\n");
}

void HandleSetCommand(char* args)
{
    char buffer[128];
    strncpy(buffer, args, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* field = strtok(buffer, " ");
    char* value = strtok(NULL, "");

    if (!field || !value) {
        LOGI("Usage: set <field> <value>\r\n");
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
            LOGI("Set %s to %s\r\n", field, value);
            if (Config_Save(&g_ConfigStore, CONFIG_FILE_PATH))
            {
                LOGE("ERROR - save config file\r\n");    
            }
        } else {
            LOGE("Failed to set %s\r\n", field);
        }
    } else {
        LOGE("Unknown setting '%s'\r\n", field);
    }
}

void HandleConfigCommand(void)
{
    LOGI("Printing ConfigStore:\r\n");
    LOGI("server: %s\r\n",   Config_GetValue(&g_ConfigStore, KEY_TRACKING_SERVER_ADDR,     NULL, 0));
    LOGI("port: %s\r\n",     Config_GetValue(&g_ConfigStore, KEY_TRACKING_SERVER_PORT,     NULL, 0));
    LOGI("protocol: %s\r\n", Config_GetValue(&g_ConfigStore, KEY_TRACKING_SERVER_PROTOCOL, NULL, 0));
    LOGI("apn: %s\r\n",      Config_GetValue(&g_ConfigStore, KEY_APN, NULL, 0));
    LOGI("apn_user: %s\r\n", Config_GetValue(&g_ConfigStore, KEY_APN_USER, NULL, 0));
    LOGI("apn_pass: %s\r\n", Config_GetValue(&g_ConfigStore, KEY_APN_PASS, NULL, 0));
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
