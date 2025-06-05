#ifndef CONFIG_VALIDATION_H
#define CONFIG_VALIDATION_H

typedef bool (*ConfigValueValidator)(const char* serialized_value);
typedef const char* (*ConfigValueSerializer)(const void* value);

typedef struct {
    const char            *param_name;
    const char            *default_value;
    ConfigValueValidator   validator;
    ConfigValueSerializer  serializer;
    void                  *value;
} t_config_map;

extern const t_config_map g_config_map[];
extern const size_t       g_config_map_size;

/**
 * @brief Get a pointer to a configuration map entry by parameter name.
 *
 * @param arg_name The name of the configuration parameter.
 * @return Pointer to the t_config_map if found, or NULL if not found.
 */
t_config_map* getConfigMap(const char* arg_name);

const char* LogLevelSerializer(const void* value);

#endif // CONFIG_VALIDATION_H
