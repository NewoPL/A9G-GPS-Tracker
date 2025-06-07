#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gps_tracker.h"
#include "config_store.h"
#include "config_validation.h"
#include "minmea.h"
#include "utils.h"
#include "debug.h"
#include "gps.h"

// Validators
bool DeviceNameValidate(const char* value);
bool ServerValidate(const char* value);
bool PortValidate(const char* value);
bool ProtocolValidate(const char* value);
bool ApnValidate(const char* value);
bool ApnUserValidate(const char* value);
bool ApnPassValidate(const char* value);
bool LogLevelValidate(const char* value);
bool LogOutputValidate(const char* value);
bool GpsUereValidate(const char* value);
bool GpsLoggingValidate(const char* value);

// Serializers
const char* StringSerializer(const void* value);
const char* FloatSerializer(const void* value);
const char* ProtocolSerializer(const void* value);
const char* LogOutputSerializer(const void* value);
const char* BoolSerializer(const void* value);

const t_config_map g_config_map[] = {
    {PARAM_DEVICE_NAME,     DEFAULT_DEVICE_NAME,     DeviceNameValidate, StringSerializer,    &g_ConfigStore.device_name},
    {PARAM_SERVER_ADDR,     DEFAULT_SERVER_ADDR,     ServerValidate,     StringSerializer,    &g_ConfigStore.server_addr},
    {PARAM_SERVER_PORT,     DEFAULT_SERVER_PORT,     PortValidate,       StringSerializer,    &g_ConfigStore.server_port},
    {PARAM_SERVER_PROTOCOL, DEFAULT_SERVER_PROTOCOL, ProtocolValidate,   ProtocolSerializer,  &g_ConfigStore.server_protocol},
    {PARAM_APN,             DEFAULT_APN_VALUE,       ApnValidate,        StringSerializer,    &g_ConfigStore.apn},
    {PARAM_APN_USER,        DEFAULT_APN_USER_VALUE,  ApnUserValidate,    StringSerializer,    &g_ConfigStore.apn_user},
    {PARAM_APN_PASS,        DEFAULT_APN_PASS_VALUE,  ApnPassValidate,    StringSerializer,    &g_ConfigStore.apn_pass},
    {PARAM_LOG_LEVEL,       DEFAULT_LOG_LEVEL,       LogLevelValidate,   LogLevelSerializer,  &g_ConfigStore.logLevel},
    {PARAM_LOG_OUTPUT,      DEFAULT_LOG_OUTPUT,      LogOutputValidate,  LogOutputSerializer, &g_ConfigStore.logOutput},
    {PARAM_GPS_UERE,        DEFAULT_GPS_UERE,        GpsUereValidate,    FloatSerializer,     &g_ConfigStore.gps_uere},
    {PARAM_GPS_LOGS,        DEFAULT_GPS_LOGS,        GpsLoggingValidate, BoolSerializer,      &g_ConfigStore.gps_logging} 
};

const size_t g_config_map_size = sizeof(g_config_map)/sizeof(g_config_map[0]);

// Returns a pointer to the config map entry for a given argument name (key)
t_config_map* getConfigMap(const char* arg_name) {
    if (!arg_name) return NULL;
    for (size_t i = 0; i < g_config_map_size; ++i) {
        if (strcmp(g_config_map[i].param_name, arg_name) == 0) {
            return (t_config_map*)&g_config_map[i];
        }
    }
    return NULL;
}

// Returns true if the device name is non-empty and less than MAX_DEVICE_NAME_LENGTH
bool DeviceNameValidate(const char* value)
{
    if (!value) return false;
    size_t len = strlen(value);
    if (len > 0 && len < MAX_DEVICE_NAME_LENGTH) {
        strncpy(g_ConfigStore.device_name, value, MAX_DEVICE_NAME_LENGTH-1);
        g_ConfigStore.device_name[MAX_DEVICE_NAME_LENGTH-1] = '\0';
        return true;
    }
    return false;
}

// Server address: non-empty, less than MAX_SERVER_ADDR_LENGTH
bool ServerValidate(const char* value)
{
    if (!value) return false;
    size_t len = strlen(value);
    if (len > 0 && len < MAX_SERVER_ADDR_LENGTH) {
        strncpy(g_ConfigStore.server_addr, value, MAX_SERVER_ADDR_LENGTH-1);
        g_ConfigStore.server_addr[MAX_SERVER_ADDR_LENGTH-1] = '\0';
        return true;
    }
    return false;
}

bool ProtocolValidate(const char* value)
{
    if (!value) return false;
    if (str_case_cmp(value, "http") == 0) {
        g_ConfigStore.server_protocol = PROT_HTTP;
        return true;
    } 
    if (str_case_cmp(value, "https") == 0) {
        g_ConfigStore.server_protocol = PROT_HTTPS;
        return true;
    } 
    return false;
}

// Port: numeric, 1-65535
bool PortValidate(const char* value)
{
    if (!value) return false;
    char* endptr;
    long port = strtol(value, &endptr, 10);
    if (*endptr == '\0' && port > 0 && port <= 65535) {
        strncpy(g_ConfigStore.server_port, value, MAX_SERVER_PORT_LENGTH-1);
        g_ConfigStore.server_port[MAX_SERVER_PORT_LENGTH-1] = '\0';
        return true;
    }
    return false;
}

// APN: non-empty, less than MAX_APN_LENGTH
bool ApnValidate(const char* value)
{
    if (!value) return false;
    size_t len = strlen(value);
    if (len > 0 && len < MAX_APN_LENGTH) {
        strncpy(g_ConfigStore.apn, value, MAX_APN_LENGTH-1);
        g_ConfigStore.apn[MAX_APN_LENGTH-1] = '\0';
        return true;
    }
    return false;
}

// APN user: can be empty, but must fit MAX_APN_USER_LENGTH
bool ApnUserValidate(const char* value)
{
    if (!value) {
        g_ConfigStore.apn_user[0] = '\0';
        return true;
    }
    if (strlen(value) < MAX_APN_USER_LENGTH) {
        strncpy(g_ConfigStore.apn_user, value, MAX_APN_USER_LENGTH-1);
        g_ConfigStore.apn_user[MAX_APN_USER_LENGTH-1] = '\0';
        return true;
    }
    return false;
}

// APN pass: can be empty, but must fit MAX_APN_USER_LENGTH
bool ApnPassValidate(const char* value)
{
    if (!value) {
        g_ConfigStore.apn_pass[0] = '\0';
        return true;
    }
    if (strlen(value) < MAX_APN_USER_LENGTH) {
        strncpy(g_ConfigStore.apn_pass, value, MAX_APN_USER_LENGTH-1);
        g_ConfigStore.apn_pass[MAX_APN_USER_LENGTH-1] = '\0';
        return true;
    }
    return false;
}

// Log level: 0-3 (assuming 0=error, 1=warn, 2=info, 3=debug)
bool LogLevelValidate(const char* value)
{
    if (!value) return false;
    if (str_case_cmp(value, "none") == 0) {
        g_ConfigStore.logLevel = LOG_LEVEL_NONE;
        return true;
    } 
    if (str_case_cmp(value, "error") == 0) {
        g_ConfigStore.logLevel = LOG_LEVEL_ERROR;
        return true;
    } 
    if ((str_case_cmp(value, "warn") == 0) || (str_case_cmp(value, "warning")) == 0){
        g_ConfigStore.logLevel = LOG_LEVEL_WARN;
        return true;
    } 
    if (str_case_cmp(value, "info") == 0) {
        g_ConfigStore.logLevel = LOG_LEVEL_INFO;
        return true;
    } 
    if (str_case_cmp(value, "debug") == 0) {
        g_ConfigStore.logLevel = LOG_LEVEL_DEBUG;
        return true;
    } 
    return false;
}

// Log output: 0=UART, 1=FILE
bool LogOutputValidate(const char* value)
{
    if (!value) return false;
    if (str_case_cmp(value, "uart") == 0) {
        g_ConfigStore.logOutput = LOGGER_OUTPUT_UART;
        return true;
    } 
    if (str_case_cmp(value, "trace") == 0) {
        g_ConfigStore.logOutput = LOGGER_OUTPUT_TRACE;
        return true;
    } 
    if (str_case_cmp(value, "file") == 0) {
        g_ConfigStore.logOutput = LOGGER_OUTPUT_FILE;
        return true;
    } 
    return false;
}

// GPS UERE: float >0 and <100
bool GpsUereValidate(const char* value)
{
    if (!value) return false;

    float uere = atof(value);
    // We cannot detect conversion errors with atof()
    if (uere > 0.0f && uere < 100.0f) {
        g_ConfigStore.gps_uere = uere;
        return true;
    }
    return false;
}

bool GpsLoggingValidate(const char* value)
{
    if (!value) return false;
    if ((str_case_cmp(value, "0") == 0) ||
        (str_case_cmp(value, "disable") == 0) || 
        (str_case_cmp(value, "disabled") == 0) ||
        (str_case_cmp(value, "false") == 0))
    {
        g_ConfigStore.gps_logging = false;
        GPS_SaveLog(false, GPS_NMEA_LOG_FILE_PATH);
        return true;
    } 
    if ((str_case_cmp(value, "1") == 0) || 
        (str_case_cmp(value, "enable") == 0) ||
        (str_case_cmp(value, "enabled") == 0) ||
        (str_case_cmp(value, "true") == 0)) 
    {
        g_ConfigStore.gps_logging = true;
        GPS_SaveLog(true, GPS_NMEA_LOG_FILE_PATH);
        return true;
    }
    return false;
}

// Serializers: return a static buffer with the string representation of the value
static char serializer_buf[MAX_LINE_LENGTH];

const char* StringSerializer(const void* value)
{
    if (!value) return NULL;
    snprintf(serializer_buf, sizeof(serializer_buf), "%s", (const char*)value);
    return serializer_buf;
}

const char* FloatSerializer(const void* value)
{
    if (!value) return NULL;
    snprintf(serializer_buf, sizeof(serializer_buf), "%f", *(const float*)value);
    return serializer_buf;
}

const char* ProtocolSerializer(const void* value)
{
    if (!value) return NULL;
    switch (*(const int*)value) {
        case PROT_HTTP:  return "http";
        case PROT_HTTPS: return "https";
    }
    return "";
}

const char* LogLevelSerializer(const void* value)
{
    if (!value) return NULL;
    switch (*(const int*)value) {
        case LOG_LEVEL_ERROR: return "error";
        case LOG_LEVEL_WARN:  return "warn";
        case LOG_LEVEL_INFO:  return "info";
        case LOG_LEVEL_DEBUG: return "debug";
    }
    return "";
}

const char* LogOutputSerializer(const void* value)
{
    if (!value) return NULL;
    switch (*(const int*)value) {
        case LOGGER_OUTPUT_UART:  return "uart";
        case LOGGER_OUTPUT_TRACE: return "trace";
        case LOGGER_OUTPUT_FILE:  return "file";
    }    
    return "";
}

const char* BoolSerializer(const void* value)
{
    if (!value) return NULL;
    snprintf(serializer_buf, sizeof(serializer_buf), "%s", *((const int*)value) ? "true" : "false");
    return serializer_buf;
}
