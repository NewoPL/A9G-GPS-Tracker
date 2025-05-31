#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

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
#define KEY_LOG_OUTPUT                   "log output"
#define KEY_LOG_LEVEL                    "log level"

typedef struct {
    char key[MAX_KEY_LENGTH];
    char value[MAX_VALUE_LENGTH];
} ConfigEntry;

typedef struct {
    ConfigEntry entries[MAX_CONFIG_LINES];
    int count;
} Config;

extern Config g_ConfigStore;

void ConfigStore_Init();

char* trim_whitespace(char* str);

/**
 * Initialize the config structure.
 */
void Config_Purge(Config* config);

/**
 * Load a config file into memory.
 */
bool Config_Load(Config* config, char* filename);

/**
 * Save config entries back to file (overwrites file).
 */
bool Config_Save(Config* config, char* filename);

/**
 * Get value for a given key.
 */
char* Config_GetValue(Config* config, const char* key, char* out_buffer, size_t buffer_len);

/**
 * Set value for a key (adds if not found).
 */
bool Config_SetValue(Config* config, char* key, char* value);

/**
 * Remove a key from the config.
 */
bool Config_RemoveKey(Config* config, char* key);

#endif // CONFIG_STORE_H
