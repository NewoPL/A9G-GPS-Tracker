#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

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

#endif // CONFIG_PARSER_H
