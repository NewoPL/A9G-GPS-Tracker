#include <api_fs.h>
#include <api_sms.h>
#include <api_network.h>
#include <api_hal_pm.h>

#include "system.h"
#include "utils.h"
#include "debug.h"
#include "network.h"
#include "gps_tracker.h"
#include "config_store.h"
#include "config_commands.h"
#include "config_validation.h"

#define MODULE_TAG "Config"

typedef void (*UartCmdHandler)(char*);

static void HandleHelpCommand(char*);
static void HandleLsCommand(char*);
static void HandleRemoveFileCommand(char*);
static void HandleSetCommand(char*);
static void HandleGetCommand(char*);
static void HandleTailCommand(char*);
static void HandleRestartCommand(char*);
static void HandleNetworkActivateCommand(char*);
static void HandleNetworkStatusCommand(char*);
static void HandleNetworkDeactivateCommand(char*);
static void HandleLocationCommand(char*);
static void HandleSmsCommand(char*);
static void HandleSmsLsCommand(char*);
static void HandleSmsRmCommand(char*);

struct uart_cmd_entry {
    const char* cmd;
    const int   cmd_len;
    UartCmdHandler handler;
    const char* syntax;
    const char* help;
};

static struct uart_cmd_entry uart_cmd_table[] = {
    {"help",           4, HandleHelpCommand,            "help",                "Show this help message"},
    {"set",            3, HandleSetCommand,             "set <param> [value]", "Set value to a specified parameter. if no value provided parameter will be cleared."},
    {"get",            3, HandleGetCommand,             "get [para]",          "Print a value of a specified parameter. (for no parameter it prints all config)"},
    {"ls",             2, HandleLsCommand,              "ls [path]",           "List files in specified folder (default: /)"},
    {"rm",             2, HandleRemoveFileCommand,      "rm <file>",           "Remove file at specified path"},
    {"tail",           4, HandleTailCommand,            "tail <file> [bytes]", "Print last [bytes] of file (default: 500 bytes)"},
    {"net activate",  12, HandleNetworkActivateCommand, "net activate",        "Activate (attach and activate) the network"},
    {"net deactivate",14, HandleNetworkDeactivateCommand, "net deactivate",    "Deactivate (detach and deactivate) the network"},
    {"net status",    10, HandleNetworkStatusCommand,   "net status",          "Print network status"},
    {"sms",            3, HandleSmsCommand,             "sms",                 "Show SMS storage info (default)"},
    {"sms ls",         6, HandleSmsLsCommand,           "sms ls <all|read|unread>", "list SMS messages ()"},
    {"sms rm",         6, HandleSmsRmCommand,           "sms rm <index|all>",  "remove SMS message (rm <index>) or remove all messages (rm all)"},
    {"location",       8, HandleLocationCommand,        "location",            "Show the last known GPS position"},
    {"restart",        7, HandleRestartCommand,         "restart",             "Restart the system immediately"},
};

/**
 * This variable holds the size of the uart_cmd_sorted_idx table.
 * It is initialized in InitUartCmdTableSortedIdx() and used to determine if the table is initialised.
 */
static size_t   uart_cmd_table_size = 0;

/**
 * This array holds the sorted indices of the uart_cmd_table. It is used for the commnad lookup and it ensures that longer (more specific) 
 * commands are matched before shorter ones. This prevents ambiguous matches when one command is a prefix of another 
 * (for example, `net` and `net activate`). By sorting the command table by command length (descending), 
 * the command parser always checks for the most specific match first, resulting in correct and predictable command handling.
 */
static uint32_t uart_cmd_sorted_idx[sizeof(uart_cmd_table)/sizeof(uart_cmd_table[0])];

/**
 * Initializes the sorted index for the UART command table.
 * This function sorts the command table based on command length in descending order.
 * It uses a simple selection sort algorithm, which is efficient for small arrays.
 */
static void InitUartCmdTableSortedIdx(void)
{
    uart_cmd_table_size = sizeof(uart_cmd_table)/sizeof(uart_cmd_table[0]);
    for (unsigned i = 0; i < uart_cmd_table_size; ++i) uart_cmd_sorted_idx[i] = i;
    // Simple selection sort for small table
    for (unsigned i = 0; i < uart_cmd_table_size-1; ++i) {
        unsigned maxj = i;
        for (unsigned j = i+1; j < uart_cmd_table_size; ++j) {
            if (uart_cmd_table[uart_cmd_sorted_idx[j]].cmd_len > uart_cmd_table[uart_cmd_sorted_idx[maxj]].cmd_len)
                maxj = j;
        }
        if (maxj != i) {
            unsigned tmp = uart_cmd_sorted_idx[i];
            uart_cmd_sorted_idx[i] = uart_cmd_sorted_idx[maxj];
            uart_cmd_sorted_idx[maxj] = tmp;
        }
    }
}

static void HandleSetCommand(char* param)
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

static void HandleGetCommand(char* param)
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

static void HandleLsCommand(char* path)
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

static void HandleRemoveFileCommand(char* path)
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

static void HandleTailCommand(char* args)
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

static void HandleLocationCommand(char* param)
{
    // First check if GPS is active
    if (!IS_GPS_STATUS_ON()) {
        UART_Printf("GPS is not active.\r\n");
        return;
    }   
    gps_PrintLocation(LOGGER_OUTPUT_UART);
    return;
}

static void HandleNetworkStatusCommand(char* param)
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
    // Print cell info using network module function
    NetworkPrintCellInfo();
}

static void HandleNetworkActivateCommand(char* param)
{
    if (NetworkAttachActivate()) {
        UART_Printf("Network activated.\r\n");
    } else {
        UART_Printf("Network activation failed.\r\n");
    }
}

static void HandleNetworkDeactivateCommand(char* param)
{
    if (Network_StartDeactive(1)) {
        UART_Printf("Network deactivated.\r\n");
    } else {
        UART_Printf("Network deactivation failed.\r\n");
    }
}

static void HandleRestartCommand(char* args)
{
    UART_Printf("System restarting...\r\n");
    PM_Restart();
}

static void HandleHelpCommand(char* args)
{
    UART_Printf("\r\nAvailable commands:\r\n");
    for (unsigned i = 0; i < sizeof(uart_cmd_table)/sizeof(uart_cmd_table[0]); ++i) {
        UART_Printf("  %-24s- %s\r\n", uart_cmd_table[i].syntax, uart_cmd_table[i].help);
    }
    UART_Printf("\r\n<param> is one of:\r\n  ");
    for (size_t i = 0; i < g_config_map_size; ++i) {
        UART_Printf("%s, ", g_config_map[i].param_name);
    }
    UART_Printf("\r\n");
}

static void HandleSmsCommand(char* param)
{
    param = trim_whitespace(param);
    if (*param != '\0') {
        UART_Printf("Unknown sms command parameter\r\n");
        return;
    }
    // Print all SMS storage info
    SMS_Storage_Info_t info;
    if (!SMS_GetStorageInfo(&info, SMS_STORAGE_SIM_CARD)) {
        UART_Printf("Failed to get SMS storage info.\r\n");
        return;
    }
    UART_Printf("SMS Storage Info (SIM):\r\n");
    UART_Printf("  Used: %d\r\n", info.used);
    UART_Printf("  Total: %d\r\n", info.total);
    UART_Printf("  Unread: %d\r\n", info.unReadRecords);
    UART_Printf("  Read: %d\r\n", info.readRecords);
    UART_Printf("  Sent: %d\r\n", info.sentRecords);
    UART_Printf("  Unsent: %d\r\n", info.unsentRecords);
    UART_Printf("  Unknown: %d\r\n", info.unknownRecords);
    UART_Printf("  Storage ID: %d\r\n", info.storageId);
    return;
}

static void HandleSmsLsCommand(char* param)
{
    SMS_Status_t status;
    param = trim_whitespace(param);
    if ((*param == '\0') || (strncmp(param, "all", 3) == 0)) {
        status = SMS_STATUS_ALL;
    }  else if (strncmp(param, "read", 4) == 0) {
        status = SMS_STATUS_READ;
    } else if (strncmp(param, "unread", 6) == 0) {
        status = SMS_STATUS_UNREAD;
    } else {
        UART_Printf("Unknown sms ls parameter. Use 'all', 'read', or 'unread'\r\n");
        return;
    }
    if (!SMS_ListMessageRequst(status, SMS_STORAGE_SIM_CARD)) {
        UART_Printf("Failed to request SMS list\r\n");
    }
    return;
}

static void HandleSmsRmCommand(char* param)
{
    param = trim_whitespace(param);
    if (*param != '\0') {
        UART_Printf("incorrect parameter\r\n");
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
}


void HandleUartCommand(char* cmd)
{
    cmd = trim_whitespace(cmd);
    if (uart_cmd_table_size == 0) InitUartCmdTableSortedIdx();
    for (unsigned k = 0; k < uart_cmd_table_size; ++k) {
        unsigned i = uart_cmd_sorted_idx[k];
        const char* c = uart_cmd_table[i].cmd;
        int len = uart_cmd_table[i].cmd_len;
        if (strncmp(cmd, c, len) == 0 && (cmd[len] == ' ' || cmd[len] == '\0')) {
            uart_cmd_table[i].handler(cmd + len);
            return;
        }
    }
    UART_Printf("Unknown command. Type 'help' to see available commands.\r\n");
}

// Print SMS list message
void SmsListMessageCallback(SMS_Message_Info_t* msg)
{
    UART_Printf("\r\n[SMS index: %d]\r\nFrom: %s\r\nTime: %u/%02u/%02u,%02u:%02u:%02u+%02d\r\nContent: %.*s\r\n\r\n",
                msg->index,
                msg->phoneNumber,
                msg->time.year, msg->time.month, msg->time.day,
                msg->time.hour, msg->time.minute, msg->time.second,
                msg->time.timeZone,
                msg->dataLen, msg->data ? (char*)msg->data : "");
}
