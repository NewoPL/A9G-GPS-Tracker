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
#define KEY_GPS_UERE                     "gps uere"

typedef struct {
    char key[MAX_KEY_LENGTH];
    char value[MAX_VALUE_LENGTH];
} ConfigEntry;

typedef struct {
    ConfigEntry entries[MAX_CONFIG_LINES];
    int count;
} Config;

/**
 * @brief Global configuration store instance.
 *
 * This global variable holds all key-value configuration entries for the application.
 * It is initialized at startup by ConfigStore_Init() and used throughout the codebase
 * to access, modify, load, and save configuration parameters.
 */
extern Config g_ConfigStore;

/**
 * @brief Initialize the global configuration store.
 *
 * This function resets the configuration store to default values and loads
 * configuration from persistent storage if available. It should be called
 * during system startup to ensure the configuration is ready for use.
 */
void ConfigStore_Init();

/**
 * @brief Clear all entries in the config store.
 *
 * This function removes all key-value pairs from the given Config structure,
 * resetting its count to zero. It does not delete the structure itself.
 * Call this before reinitializing or reloading configuration data.
 */
void Config_Purge(Config* config);

/**
 * @brief Load a config file into memory.
 *
 * This function reads key-value pairs from the specified configuration file
 * and populates the given Config structure. Existing entries in the config
 * are cleared before loading. Returns true on success, false on failure.
 *
 * @param config Pointer to the Config structure to populate.
 * @param filename Path to the configuration file to load.
 * @return true if the file was loaded successfully, false otherwise.
 */
bool Config_Load(Config* config, char* filename);

/**
 * @brief Save config entries back to file (overwrites file).
 *
 * This function writes all key-value pairs from the given Config structure
 * to the specified file, overwriting any existing content. Returns true on
 * success, or false if the file could not be written.
 *
 * @param config Pointer to the Config structure to save.
 * @param filename Path to the configuration file to write.
 * @return true if the file was saved successfully, false otherwise.
 */
bool Config_Save(Config* config, char* filename);

/**
 * @brief Get value for a given key from the config store.
 *
 * This function searches the provided Config structure for the specified key.
 * If found, it returns a pointer to the value string. If out_buffer is provided,
 * the value is also copied into it (up to buffer_len bytes, including null terminator).
 * Returns NULL if the key is not found.
 *
 * @param config Pointer to the Config structure to search.
 * @param key The key to look up.
 * @param out_buffer Optional buffer to copy the value into (can be NULL).
 * @param buffer_len Size of the out_buffer.
 * @return Pointer to the value string if found, or NULL if not found.
 */
char* Config_GetValue(Config* config, const char* key, char* out_buffer, size_t buffer_len);

float Config_GetValueFloat(Config* config, const char* key);

/**
 * @brief Set value for a key in the config store (adds if not found).
 *
 * This function sets the value for the specified key in the given Config structure.
 * If the key already exists, its value is updated. If the key does not exist and
 * there is space, a new entry is added. Returns true on success, or false if the
 * config is full or another error occurs.
 *
 * @param config Pointer to the Config structure to modify.
 * @param key The key to set or add.
 * @param value The value to associate with the key.
 * @return true if the value was set or added successfully, false otherwise.
 */
bool Config_SetValue(Config* config, char* key, char* value);

/**
 * @brief Remove a key from the config store.
 *
 * This function searches for the specified key in the given Config structure
 * and removes the corresponding key-value pair if found. The config is compacted
 * to fill the gap left by the removed entry. Returns true if the key was found
 * and removed, or false if the key was not present.
 *
 * @param config Pointer to the Config structure to modify.
 * @param key The key to remove from the config.
 * @return true if the key was found and removed, false otherwise.
 */
bool Config_RemoveKey(Config* config, char* key);

char* trim_whitespace(char* str);

#endif // CONFIG_STORE_H
