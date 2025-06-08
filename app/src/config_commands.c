#include <stdio.h>
#include <stdlib.h>

#include <api_fs.h>
#include <api_sms.h>
#include <api_network.h>
#include <api_hal_pm.h>

#include "gps_parse.h"
#include "system.h"
#include "network.h"
#include "gps_tracker.h"
#include "config_store.h"
#include "config_commands.h"
#include "config_validation.h"
#include "minmea.h"
#include "utils.h"
#include "debug.h"

typedef void (*UartCmdHandler)(char*);

void HandleHelpCommand(char*);
void HandleLsCommand(char*);
void HandleRemoveFileCommand(char*);
void HandleSetCommand(char*);
void HandleGetCommand(char*);
void HandleTailCommand(char*);
void HandleRestartCommand(char*);
void HandleNetworkActivateCommand(char*);
void HandleNetworkStatusCommand(char*);
void HandleLocationCommand(char*);
void HandleSmsCommand(char*);

struct uart_cmd_entry {
    const char* cmd;
    int cmd_len;
    UartCmdHandler handler;
    const char* syntax;
    const char* help;
};

static struct uart_cmd_entry uart_cmd_table[] = {
    {"help",         4, HandleHelpCommand,            "help",                "Show this help message"},
    {"set",          3, HandleSetCommand,             "set <param> [value]", "Set value to a specified parameter. if no value provided parameter will be cleared."},
    {"get",          3, HandleGetCommand,             "get [para]",          "Print a value of a specified parameter. (for no parameter it prints all config)"},
    {"ls",           2, HandleLsCommand,              "ls [path]",           "List files in specified folder (default: /)"},
    {"rm",           2, HandleRemoveFileCommand,      "rm <file>",           "Remove file at specified path"},
    {"tail",         4, HandleTailCommand,            "tail <file> [bytes]", "Print last [bytes] of file (default: 500 bytes)"},
    {"restart",      7, HandleRestartCommand,         "restart",             "Restart the system immediately"},
    {"netactivate", 11, HandleNetworkActivateCommand, "netactivate",         "Activate (attach and activate) the network"},
    {"netstatus",    9, HandleNetworkStatusCommand,   "netstatus",           "Print network status"},
    {"location",     8, HandleLocationCommand,        "location",            "Show the last known GPS position"},
    {"sms",          3, HandleSmsCommand,             "sms [ls|rm <index>]", "List or remove SMS messages (default: ls)"},
};


void HandleSetCommand(char* param)
{
    param = trim_whitespace(param);
    if (!*param) {
        UART_Printf("missing variable\r\n");
        return;
    }

    // Find the first space (if any)
    char* space = strchr(param, ' ');
    char* value = NULL;
    if (space) {
        *space = '\0';
        value = space + 1;
    } else {
        // No value provided, set to empty string
        value = (char*)"";
    }

    t_config_map *configMap = getConfigMap(param);
    if (!configMap) {
        UART_Printf("unknown variable\r\n");
        return;
    }

    if (!configMap->validator(value)) {
        UART_Printf("invalid value for %s\r\n", param);
        return;
    }

    if (!ConfigStore_Save(CONFIG_FILE_PATH))
        LOGE("Failed to save config file: %s", CONFIG_FILE_PATH);

    UART_Printf("Set %s to '%s'\r\n", param, value);
    return;
}

void HandleGetCommand(char* param)
{
    param = trim_whitespace(param);

    if (*param == '\0') {
        UART_Printf("Current configuration:\r\n");
        for (size_t i = 0; i < g_config_map_size; ++i) {
            const t_config_map *configMap = &g_config_map[i];
            UART_Printf("%-20s : %s\r\n",
                configMap->param_name, 
                configMap->serializer(configMap->value));
        }
        return;
    }

    t_config_map *configMap = getConfigMap(param);
    if (!configMap) {
        UART_Printf("unknown variable\r\n");
        return;
    }
    UART_Printf("%s : %s\r\n",
        configMap->param_name, 
        configMap->serializer(configMap->value));
    return;
}

void HandleLsCommand(char* path)
{
    path = trim_whitespace(path);
    if (!path || path[0] == '\0') path = "/";

    UART_Printf("File list in %s:\r\n", path);
    Dir_t* dir = API_FS_OpenDir(path);
    if (!dir) {
        UART_Printf("cannot open directory: %s\r\n", path);
        return;
    }

    char file_path[256];
    strncpy(file_path, path, sizeof(file_path) - 1);
    file_path[sizeof(file_path) - 1] = '\0';
    size_t base_path_len = strlen(file_path);
    // Ensure file_path ends with '/'
    if (base_path_len == 0 || file_path[base_path_len - 1] != '/') {
        if (base_path_len < sizeof(file_path) - 1) {
            file_path[base_path_len] = '/';
            file_path[base_path_len + 1] = '\0';
            base_path_len++;
        }
    }

    Dirent_t* dirent = NULL;

    while ((dirent = API_FS_ReadDir(dir)))
    {
        if (dirent->d_type == 8)
        {
            int32_t  file_size = 0;
            strncpy(file_path + base_path_len, dirent->d_name, sizeof(file_path) - base_path_len - 1);
            file_path[base_path_len + strlen(dirent->d_name)] = '\0';
            int32_t fd = API_FS_Open(file_path, FS_O_RDONLY, 0);
            if(fd < 0)
                LOGE("open file %s fail", file_path);
            else
            {
                file_size = (int)API_FS_GetFileSize(fd);
                API_FS_Close(fd);
            }
            UART_Printf("<file>  %-*s  %8d\r\n", 20, dirent->d_name, file_size);
        } else if (dirent->d_type == 4) {
            UART_Printf("<dir>   %-*s\r\n", 20, dirent->d_name);
        }
        else 
            UART_Printf("<unknown>\r\n");
    }
    API_FS_CloseDir(dir);
}

void HandleRemoveFileCommand(char* path)
{
    path = trim_whitespace(path);
    if (!path || path[0] == '\0') {
        UART_Printf("no file specified\r\n");
        return;
    }
    int result = API_FS_Delete(path);
    if (result == 0) {
        UART_Printf("File deleted: %s\r\n", path);
    } else {
        UART_Printf("failed to delete file: %s\r\n", path);
    }
}

void HandleTailCommand(char* args)
{
    int bytes = 500;
    char* file_path = trim_whitespace(args);
    if (!file_path || !*file_path) {
        UART_Printf("missing file\r\n");
        return;
    }

    char* space = strchr(file_path, ' ');
    if (space) {
        *space = '\0';
        char* bytes_str = trim_whitespace(space + 1);
        if (bytes_str && *bytes_str) {
            int val = atoi(bytes_str);
            if (val > 0) bytes = val;
        }
    }

    int32_t fd = API_FS_Open(file_path, FS_O_RDONLY, 0);
    if (fd < 0) {
        UART_Printf("tail: cannot open file %s\r\n", file_path);
        return;
    }
    int file_size = (int)API_FS_GetFileSize(fd);
    if (file_size < 0) {
        UART_Printf("tail: cannot get file size\r\n");
        API_FS_Close(fd);
        return;
    }
    int start = file_size > bytes ? file_size - bytes : 0;
    if (API_FS_Seek(fd, start, 0) < 0) {
        UART_Printf("tail: seek error\r\n");
        API_FS_Close(fd);
        return;
    }
    char buf[128];
    int remaining = file_size - start;
    while (remaining > 0) {
        int to_read = remaining > (int)sizeof(buf) ? (int)sizeof(buf) : remaining;
        int n = API_FS_Read(fd, buf, to_read);
        if (n <= 0) break;
        for (int i = 0; i < n; ++i) UART_Printf("%c", buf[i]);
        remaining -= n;
    }
    UART_Printf("\r\n");
    API_FS_Close(fd);
}

void HandleLocationCommand(char* param)
{
    GPS_Info_t* gpsInfo = Gps_GetInfo();
    
    // First check if GPS is active
    if (!IS_GPS_STATUS_ON()) {
        UART_Printf("GPS is not active.\r\n");
        return;
    }

    if (!gpsInfo->rmc.valid) {
        UART_Printf("No valid GPS fix available.\r\n");
        UART_Printf("Satellites visible: %d, tracked: %d\r\n", 
                    gpsInfo->gsv[0].total_sats, 
                    gpsInfo->gga.satellites_tracked);
    }

    // Convert NMEA coordinates (DDMM.MMMM format) to decimal degrees
    float latitude = minmea_tocoord(&gpsInfo->rmc.latitude);
    float longitude = minmea_tocoord(&gpsInfo->rmc.longitude);
    
    // Format and display GPS information
    UART_Printf("GPS Position:\r\n");
    UART_Printf("  Date:        %02d.%02d.%02d\r\n", gpsInfo->rmc.date.year, gpsInfo->rmc.date.month, gpsInfo->rmc.date.day);
    UART_Printf("  Time:        %02d.%02d.%02d\r\n", gpsInfo->rmc.time.hours, gpsInfo->rmc.time.minutes, gpsInfo->rmc.time.seconds);
    UART_Printf("  Latitude:    %.6f° %c\r\n", fabs(latitude), (latitude >= 0) ? 'N' : 'S');
    UART_Printf("  Longitude:   %.6f° %c\r\n", fabs(longitude), (longitude >= 0) ? 'E' : 'W');
    UART_Printf("  Altitude:    %.1f meters\r\n", gpsInfo->gga.altitude);
    UART_Printf("  Speed:       %.1f km/h\r\n", minmea_tofloat(&gpsInfo->rmc.speed) * 1.852); // Convert knots to km/h
    UART_Printf("  Course:      %.1f°\r\n", minmea_tofloat(&gpsInfo->rmc.course));
    UART_Printf("  Fix quality: %d\r\n", gpsInfo->gga.fix_quality);
    UART_Printf("  HDOP: %.1f\r\n", minmea_tofloat(&gpsInfo->gsa[0].hdop));
}

void HandleNetworkStatusCommand(char* param)
{
    UART_Printf("GSM Network registered: %s, active: %s\r\n",
                IS_GSM_REGISTERED() ? "true" : "false",
                IS_GSM_ACTIVE() ? "true" : "false");
    
    // Get and display IP address if network is active
    if (IS_GSM_ACTIVE()) {
        char ip_address[16] = {0};
        if (Network_GetIp(ip_address, sizeof(ip_address))) {
            UART_Printf("IP address: %s\r\n", ip_address);
        } else {
            UART_Printf("Failed to get IP address\r\n");
        }
    } else {
        UART_Printf("IP address: not available\r\n");
    }

    if (g_cellInfo[0] != 0) {
        UART_Printf("Cell info: %s\r\n", g_cellInfo);
        // Try to parse and print cell info fields if present
        int mcc = 0, mnc = 0, lac = 0, cellid = 0, rxlev = 0;
        if (sscanf(g_cellInfo, "%3d,%3d,%d,%d,%d", &mcc, &mnc, &lac, &cellid, &rxlev) == 5) {
            UART_Printf("  MCC: %03d\r\n  MNC: %03d\r\n  LAC: %d\r\n  CellID: %d\r\n  RxLev: %d\r\n",
                mcc, mnc, lac, cellid, rxlev);
        }
    } else
        UART_Printf("Cell info not available\r\n");
}

void HandleNetworkActivateCommand(char* param)
{
    if (gsm_AttachActivate()) {
        UART_Printf("Network activated.\r\n");
    } else {
        UART_Printf("Network activation failed.\r\n");
    }
}

void HandleRestartCommand(char* args)
{
    UART_Printf("System restarting...\r\n");
    PM_Restart();
}

void HandleHelpCommand(char* args)
{
    UART_Printf("\r\nAvailable commands:\r\n");
    for (unsigned i = 0; i < sizeof(uart_cmd_table)/sizeof(uart_cmd_table[0]); ++i) {
        UART_Printf("  %-24s- %s\r\n", uart_cmd_table[i].syntax, uart_cmd_table[i].help);
    }
    UART_Printf("\r\n  <param> is one of:\r\n");
    for (size_t i = 0; i < g_config_map_size; ++i) {
        UART_Printf("    %s\r\n", g_config_map[i].param_name);
    }
}

void HandleSmsCommand(char* param)
{
    param = trim_whitespace(param);
    if (*param == '\0' || strncmp(param, "ls", 2) == 0) {
        // List all SMS messages
        if (!SMS_ListMessageRequst(SMS_STATUS_ALL, SMS_STORAGE_SIM_CARD)) {
            UART_Printf("Failed to request SMS list.\r\n");
            return;
        }
    } else if (strncmp(param, "rm", 2) == 0) {
        // Remove SMS message by index
        param += 2;
        param = trim_whitespace(param);
        if (*param == '\0') {
            UART_Printf("Usage: sms rm <index>\r\n");
            return;
        }
        int idx = atoi(param);
        if (idx < 0) {
            UART_Printf("Invalid index.\r\n");
            return;
        }
        if (SMS_DeleteMessage(idx, SMS_STATUS_ALL, SMS_STORAGE_SIM_CARD)) {
            UART_Printf("Deleted SMS at index %d\r\n", idx);
        } else {
            UART_Printf("Failed to delete SMS at index %d\r\n", idx);
        }
    } else {
        UART_Printf("Unknown sms subcommand. Use 'sms', 'sms ls', or 'sms rm <index>'\r\n");
    }
}

// Print SMS list message
void HandleSmsListEvent(SMS_Message_Info_t* msg)
{
    UART_Printf("\r\n[SMS index: %d]\r\nFrom: %s\r\nTime: %u/%02u/%02u,%02u:%02u:%02u+%02d\r\nContent: %.*s\r\n\r\n",
                msg->index,
                msg->phoneNumber,
                msg->time.year, msg->time.month, msg->time.day,
                msg->time.hour, msg->time.minute, msg->time.second,
                msg->time.timeZone,
                msg->dataLen, msg->data ? (char*)msg->data : "");
}

void HandleUartCommand(char* cmd)
{
    cmd = trim_whitespace(cmd);
    for (unsigned i = 0; i < sizeof(uart_cmd_table)/sizeof(uart_cmd_table[0]); ++i) {
        const char* c = uart_cmd_table[i].cmd;
        int len = uart_cmd_table[i].cmd_len;
        if (strncmp(cmd, c, len) == 0 && (cmd[len] == ' ' || cmd[len] == '\0')) {
            uart_cmd_table[i].handler(cmd + len);
            return;
        }
    }
    UART_Printf("Unknown command. Type 'help' to see available commands.\r\n");
}
