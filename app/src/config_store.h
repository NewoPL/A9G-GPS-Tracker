#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#define MAX_LINE_LENGTH             256
#define MAX_SERVER_ADDR_LENGTH      64
#define MAX_SERVER_PORT_LENGTH      8
#define MAX_SERVER_PROTOCOL_LENGTH  16
#define MAX_APN_LENGTH              32
#define MAX_APN_USER_LENGTH         32
#define MAX_DEVICE_NAME_LENGTH      32
#define MAX_IMEI_LENGTH             16

#define PARAM_DEVICE_NAME           "device_name"
#define PARAM_SERVER_ADDR           "server"
#define PARAM_SERVER_PORT           "port"
#define PARAM_SERVER_PROTOCOL       "protocol"
#define PARAM_APN                   "apn"
#define PARAM_APN_USER              "apn_user"
#define PARAM_APN_PASS              "apn_pass"
#define PARAM_LOG_LEVEL             "log_level"
#define PARAM_LOG_OUTPUT            "log_output"
#define PARAM_GPS_UERE              "gps_uere"
#define PARAM_GPS_LOGS              "gps_logging"

typedef struct {
    char  imei[MAX_IMEI_LENGTH];
    char  device_name[MAX_DEVICE_NAME_LENGTH];
    char  server_addr[MAX_SERVER_ADDR_LENGTH];
    char  server_port[MAX_SERVER_PORT_LENGTH];
    char  server_protocol[MAX_SERVER_PROTOCOL_LENGTH];
    char  apn[MAX_APN_LENGTH];
    char  apn_user[MAX_APN_USER_LENGTH];
    char  apn_pass[MAX_APN_USER_LENGTH];
    float gps_uere;
    bool  gps_logging;
    t_logLevel  logLevel;
    t_logOutput logOutput;
} t_Config;


/**
 * @brief Global configuration store instance.
 *
 * This global variable holds all key-value configuration entries for the application.
 * It is initialized at startup by ConfigStore_Init() and used throughout the codebase
 * to access, modify, load, and save configuration parameters.
 */
extern t_Config g_ConfigStore;

/**
 * @brief Initialize the global configuration store.
 *
 * This function resets the configuration store to default values and loads
 * configuration from persistent storage if available. It should be called
 * during system startup to ensure the configuration is ready for use.
 */
void ConfigStore_Init(void);

/**
 * @brief Save config entries back to file (overwrites file).
 *
 * This function writes all key-value pairs from the global t_Config structure
 * to the specified file, overwriting any existing content. Returns true on
 * success, or false if the file could not be written.
 *
 * @param filename Path to the configuration file to write.
 * @return true if the file was saved successfully, false otherwise.
 */
bool ConfigStore_Save(char* filename);

/**
 * @brief Trim leading and trailing whitespace from a string in place.
 *
 * @param str The string to trim.
 * @return Pointer to the trimmed string.
 */
char* trim_whitespace(char* str);

#endif // CONFIG_STORE_H
