#include <stdio.h>
#include <api_info.h>

#include "config.h"
#include "config_parser.h"
#include "gps_tracker.h"

Config ConfigStore;

void ConfigStore_Init()
{
    Config_Purge(&ConfigStore);
    Config_SetValue(&ConfigStore, KEY_APN, DEFAULT_APN_VALUE);
    Config_SetValue(&ConfigStore, KEY_APN_USER, DEFAULT_APN_USER_VALUE);
    Config_SetValue(&ConfigStore, KEY_APN_PASS, DEFAULT_APN_PASS_VALUE);
    Config_SetValue(&ConfigStore, KEY_TRACKING_SERVER_ADDR, DEFAULT_TRACKING_SERVER_ADDR);
    Config_SetValue(&ConfigStore, KEY_TRACKING_SERVER_PORT, DEFAULT_TRACKING_SERVER_PORT);
    Config_SetValue(&ConfigStore, KEY_TRACKING_SERVER_PROTOCOL, DEFAULT_TRACKING_SERVER_PROTOCOL);

    if (!Config_Load(&ConfigStore, CONFIG_FILE_PATH))
    {
        Trace(1,"load config file fail");
    }

    char *device_name = Config_GetValue(&ConfigStore, KEY_DEVICE_NAME, NULL, 0);
    if ((device_name == NULL) || (device_name[0]='\0'))
    {
        char IMEI[16];
        memset(IMEI, 0, sizeof(IMEI));
        if(INFO_GetIMEI(IMEI))
        {
            Config_SetValue(&ConfigStore, KEY_DEVICE_NAME, IMEI);
        } else {
            Config_SetValue(&ConfigStore, KEY_DEVICE_NAME, DEFAULT_DEVICE_NAME);
        }
    }
}

void HandleUartCommand(const char* cmd)
{
    if (strcmp(cmd, "config") == 0)
    {
        UART_Log("Printing ConfigStore:\r\n");
        UART_Log("server: %s\r\n",   Config_GetValue(&ConfigStore, KEY_TRACKING_SERVER_ADDR,     NULL, 0));
        UART_Log("port: %s\r\n",     Config_GetValue(&ConfigStore, KEY_TRACKING_SERVER_PORT,     NULL, 0));
        UART_Log("protocol: %s\r\n", Config_GetValue(&ConfigStore, KEY_TRACKING_SERVER_PROTOCOL, NULL, 0));
        UART_Log("apn: %s\r\n",      Config_GetValue(&ConfigStore, KEY_APN, NULL, 0));
        UART_Log("apn_user: %s\r\n", Config_GetValue(&ConfigStore, KEY_APN_USER, NULL, 0));
        UART_Log("apn_pass: %s\r\n", Config_GetValue(&ConfigStore, KEY_APN_PASS, NULL, 0));
    }
    else {
        UART_Log("Unknown UART command: %s", cmd);
    }
}
