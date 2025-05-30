#ifndef CONFIG_H
#define CONFIG_H

#define MAX_LINE_LENGTH                  256
#define MAX_CONFIG_LINES                 64
#define MAX_KEY_LENGTH                   64
#define MAX_VALUE_LENGTH                 128

#define KEY_APN                          "apn"
#define KEY_APN_USER                     "apn user"
#define KEY_APN_PASS                     "apn password"
#define KEY_TRACKING_SERVER_ADDR         "tracking server address"       
#define KEY_TRACKING_SERVER_PORT         "tracking server port"
#define KEY_TRACKING_SERVER_PROTOCOL     "tracking server protocol"
#define KEY_DEVICE_NAME                  "device name"

typedef struct {
    char key[MAX_KEY_LENGTH];
    char value[MAX_VALUE_LENGTH];
} ConfigEntry;

typedef struct {
    ConfigEntry entries[MAX_CONFIG_LINES];
    int count;
} Config;

extern Config g_ConfigStore;

void FsInfoTest();
void ConfigStore_Init();
void HandleUartCommand(char* cmd);


#endif
