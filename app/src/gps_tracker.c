#include <string.h>
#include <stdio.h>
#include <api_os.h>
#include <api_gps.h>
#include <api_info.h>
#include <api_event.h>
#include <api_network.h>
#include <api_fs.h>
#include <api_lbs.h>
#include <api_hal_uart.h>
#include <api_hal_pm.h>

#include "math.h"
#include "time.h"
#include "assert.h"
#include "buffer.h"
#include "gps.h"
#include "gps_parse.h"

#include "system.h"
#include "gps_tracker.h"
#include "config_store.h"
#include "config_commands.h"
#include "led_handler.h"
#include "network.h"
#include "utils.h"
#include "debug.h"

#define MAIN_TASK_STACK_SIZE    (2048 * 2)
#define MAIN_TASK_PRIORITY      0
#define MAIN_TASK_NAME          "GPS Tracker"

static HANDLE gpsTaskHandle = NULL;
Network_PDP_Context_t NetContext;
uint8_t systemStatus = 0;
uint8_t g_RSSI = 0;
char    g_cellInfo[128] = "\0";

SSL_Config_t SSLconfig = {
    .caCert          = NULL,
    .caCrl           = NULL,
    .clientCert      = NULL,
    .clientKey       = NULL,
    .clientKeyPasswd = NULL,
    .hostName        = NULL,
    .minVersion      = SSL_VERSION_SSLv3,
    .maxVersion      = SSL_VERSION_TLSv1_2,
    .verifyMode      = SSL_VERIFY_MODE_OPTIONAL,
    .entropyCustom   = "GPRS"
};

bool AttachActivate()
{
    uint8_t status;
    bool ret = Network_GetAttachStatus(&status);
    if(!ret)
    {
        LOGE("get attach status failed");
        return false;
    }
    if(!status)
    {
        ret = Network_StartAttach();
        LOGI("attaching to the network");
        if(!ret)
        {
            LOGE("network attach failed");
            return false;
        }
    }
    else
    {
        ret = Network_GetActiveStatus(&status);
        if(!ret)
        {
            LOGE("get activate status failed");
            return false;
        }
        if(!status)
        {
            LOGI("activating the network");
            strncpy(NetContext.apn,      g_ConfigStore.apn,      sizeof(NetContext.apn));
            strncpy(NetContext.userName, g_ConfigStore.apn_user, sizeof(NetContext.userName));
            strncpy(NetContext.userPasswd, g_ConfigStore.apn_pass, sizeof(NetContext.userPasswd));
            ret = Network_StartActive(NetContext);
            if(!ret) {
               LOGE("network activate failed");
               return false;
            }
        }
    }
    return true;
}

uint8_t csq_to_percent(int csq)
{
    if (csq < 0 || csq == 99) {
        // 99 means unknown or undetectable signal
        return 0;
    }
    if (csq > 31) {
        csq = 31;  // Clamp max value
    }
    // Scale 0–31 to 0–100%
    return (csq * 100) / 31;
}

void processNetworkCellInfo(Network_Location_t* loc, int number)
{
    g_cellInfo[0] = '\0';
    if (number<=0) return;
    snprintf(g_cellInfo,  sizeof(g_cellInfo), "%u%u%u,%u%u%u,%u,%u,%d",
             loc->sMcc[0], loc->sMcc[1], loc->sMcc[2], loc->sMnc[0], loc->sMnc[1], loc->sMnc[2], loc->sLac, loc->sCellID, loc->iRxLev);
}

static void format_size(char* buf, size_t bufsize, int value, const char* label) {
    if (value >= 1024*1024*1024) {
        float gb = value / (1024.0f * 1024.0f * 1024.0f);
        snprintf(buf, bufsize, "%s%.3f GB", label, gb);
    } else if (value >= 1024*1024) {
        float mb = value / (1024.0f * 1024.0f);
        snprintf(buf, bufsize, "%s%.1f MB", label, mb);
    } else if (value >= 1024) {
        float kb = value / 1024.0f;
        snprintf(buf, bufsize, "%s%.1f KB", label, kb);
    } else {
        snprintf(buf, bufsize, "%s%d Bytes", label, value);
    }
}

void FsInfoTest()
{
    API_FS_INFO fsInfo;
    int sizeUsed = 0, sizeTotal = 0;
    char used_buf[32], total_buf[32];

    if(API_FS_GetFSInfo(FS_DEVICE_NAME_FLASH, &fsInfo) < 0)
    {
        LOGE("Get Int Flash device info fail!");
    }
    sizeUsed  = fsInfo.usedSize;
    sizeTotal = fsInfo.totalSize;
    format_size(used_buf, sizeof(used_buf), sizeUsed, "");
    format_size(total_buf, sizeof(total_buf), sizeTotal, "");
    LOGI("Int Flash used: %s, total size: %s", used_buf, total_buf);

    if(API_FS_GetFSInfo(FS_DEVICE_NAME_T_FLASH, &fsInfo) < 0)
    {
        LOGE("Get Ext Flash device info fail!");
    }
    sizeUsed  = fsInfo.usedSize;
    sizeTotal = fsInfo.totalSize;
    format_size(used_buf, sizeof(used_buf), sizeUsed, "");
    format_size(total_buf, sizeof(total_buf), sizeTotal, "");
    LOGI("Ext Flash used: %s, total size: %s", used_buf, total_buf);
}

void EventHandler(API_Event_t* pEvent)
{
    switch(pEvent->id)
    {
        case API_EVENT_ID_NO_SIMCARD:
            GSM_STATUS_OFF();
            LOGE("no sim card %d !", pEvent->param1);
            break;
        case API_EVENT_ID_SIMCARD_DROP:
            GSM_STATUS_OFF();
            LOGE("sim card %d drop !",pEvent->param1);
            break;
        case API_EVENT_ID_NETWORK_REGISTER_SEARCHING:
            LOGW("network register searching");
            break;
        case API_EVENT_ID_NETWORK_REGISTER_DENIED:
            LOGE("network register denied");
            GSM_STATUS_OFF();
            break;
        case API_EVENT_ID_NETWORK_REGISTER_NO:
            LOGE("network register no");
            break;

        case API_EVENT_ID_NETWORK_REGISTERED_HOME:
        case API_EVENT_ID_NETWORK_REGISTERED_ROAMING:
            LOGW("network register success");
            AttachActivate();
            break;

        case API_EVENT_ID_NETWORK_ATTACHED:
            LOGW("network attach success");
            AttachActivate();
            break;

        case API_EVENT_ID_NETWORK_ACTIVATED:
            GSM_STATUS_ON();
            LOGW("network activate success");
            break;

        case API_EVENT_ID_NETWORK_ACTIVATE_FAILED:
            GSM_STATUS_OFF();
            LOGE("network activate failed");
            break;

        case API_EVENT_ID_NETWORK_DEACTIVED:
            GSM_STATUS_OFF(); 
            LOGE("network deactived");
            break;

        case API_EVENT_ID_NETWORK_ATTACH_FAILED:
            GSM_STATUS_OFF();
            LOGE("network attach failed");
            break;

        case API_EVENT_ID_NETWORK_DETACHED:
            GSM_STATUS_OFF();
            LOGE("network detached");
            break;

        case API_EVENT_ID_SIGNAL_QUALITY:
            g_RSSI = csq_to_percent(pEvent->param1);
            break;

        case API_EVENT_ID_NETWORK_CELL_INFO:
            processNetworkCellInfo((Network_Location_t*)pEvent->pParam1, pEvent->param1);
            break;

        case API_EVENT_ID_SYSTEM_READY:
            LOGW("system initialize complete");
            INITIALIZED_ON();
            break;

        case API_EVENT_ID_GPS_UART_RECEIVED:
            LOGD("received GPS data, length:%d, data:%s",pEvent->param1,pEvent->pParam1);
            GPS_Update(pEvent->pParam1, pEvent->param1);

            GPS_Info_t* gpsInfo = Gps_GetInfo();
            if (gpsInfo->rmc.valid) GPS_FIX_ON(); else GPS_FIX_OFF();

            GPS_STATUS_ON();
            break;
        
        case API_EVENT_ID_UART_RECEIVED:
            if(pEvent->param1 == UART1)
            {
                uint8_t data[(pEvent->param2 + 1)];
                data[pEvent->param2] = 0;
                memcpy(data,pEvent->pParam1,pEvent->param2);
                HandleUartCommand(data);
            }
            break;
        default:
            break;
    }
}

uint8_t requestBuffer[400];
uint8_t responseBuffer[1024];

void gps_trackingTask(void *pData)
{
    // wait for initialization
    LOGI("Initialization");
    LED_cycle_start(gpsTaskHandle);
    FsInfoTest();
    while (!IS_INITIALIZED() || !IS_GSM_STATUS_ON()) OS_Sleep(2000);

    GPS_Info_t* gpsInfo = Gps_GetInfo();
    GPS_SaveLog(false, GPS_NMEA_LOG_FILE_PATH);
 
    // open GPS hardware(UART2 open either)
    GPS_Open(NULL);
    LOGI("Waiting for GPS");
    while(!IS_GPS_STATUS_ON()) OS_Sleep(2000);

    if(!GPS_GetVersion(responseBuffer, 255))
        LOGE("get GPS firmware version failed");
    else
        LOGW("GPS firmware version: %s", responseBuffer);

    GPS_SetSearchMode(true, false, false, true);

    // if(!GPS_ClearLog())
    //    LOGE("open file failed, please check tf card");

    // if(!GPS_ClearInfoInFlash())
    //     LOGE("erase gps fail");
    
    // if(!GPS_SetQzssOutput(false))
    //     LOGE("enable qzss nmea output fail");

    //if(!GPS_SetSBASEnable(true))
    //     LOGE("enable sbas fail");

    LOGI("setting GPS fix mode to MODE_NORMAL");
    if(!GPS_SetFixMode(GPS_FIX_MODE_NORMAL))
        LOGE("set fix mode fail");

    LOGI("setting GPS LP Mode to GPS_LP_MODE_NORMAL");
    if(!GPS_SetLpMode(GPS_LP_MODE_NORMAL))
        LOGE("set GPS LP mode failed");

    LOGI("setting GPS interval to 1000 ms");
    if(!GPS_SetOutputInterval(1000))
        LOGE("set GPS interval failed");
    
    const char *device_name = g_ConfigStore.device_name;
    LOGI("Device name: %s", device_name);

    while(1)
    {
        uint32_t loop_start = time(NULL);
        if(IS_GPS_STATUS_ON() && (gpsInfo->rmc.valid))
        {
            if(!Network_GetCellInfoRequst()) {
                g_cellInfo[0] = '\0';
                LOGE("network get cell info fail");
            }

            time_t gps_timestamp = mk_time(&gpsInfo->rmc.date, &gpsInfo->rmc.time);

            // convert coordinates ddmm.mmmm to a floating point DD.DDD value in degree(°) 
            float latitude  = minmea_tocoord(&gpsInfo->rmc.latitude);
            float longitude = minmea_tocoord(&gpsInfo->rmc.longitude);
            
            // convert other data a floating point 
            float speed     = minmea_tofloat(&gpsInfo->rmc.speed);
            float bearing   = minmea_tofloat(&gpsInfo->rmc.course);
            float altitude  = minmea_tofloat(&gpsInfo->gga.altitude);
            
            float accuracy0 = minmea_tofloat(&gpsInfo->gsa[0].hdop);

            float gps_uere = g_ConfigStore.gps_uere;
            float accuracy = gps_uere * accuracy0;

            uint8_t percent;
            PM_Voltage(&percent);

            snprintf(responseBuffer, sizeof(responseBuffer),"%02d.%02d.%02d %02d:%02d.%02d, "
                                                            "sat visible:%d, sat tracked:%d, "
                                                            "Lat:%f, Lon:%f, alt:%f, "
                                                            "error = %.1f, "
                                                            "spd= %.1f, hdg= %.1f, bat= %d\r\n",
                                                            gpsInfo->rmc.date.year,gpsInfo->rmc.date.month, gpsInfo->rmc.date.day,
                                                            gpsInfo->rmc.time.hours,gpsInfo->rmc.time.minutes,gpsInfo->rmc.time.seconds,
                                                            gpsInfo->gsv[0].total_sats, gpsInfo->gga.satellites_tracked,
                                                            latitude, longitude, altitude, accuracy, speed, bearing, percent);
            responseBuffer[sizeof(responseBuffer) - 1] = '\0';

            //send to UART1
            UART_Write(UART1, responseBuffer, strlen(responseBuffer));

            responseBuffer[0] = '\0';
            if (strlen(g_cellInfo) != 0) {
                 snprintf(responseBuffer, sizeof(responseBuffer),"&cell=%s", g_cellInfo);
            }

            snprintf(requestBuffer, sizeof(requestBuffer),
                     "id=%s&valid=%d&timestamp=%d&lat=%f&lon=%f&speed=%1.f&bearing=%.1f&altitude=%.1f&accuracy=%.1f%s&batt=%d",
                     device_name, gpsInfo->rmc.valid, gps_timestamp, latitude, longitude, speed, bearing, altitude, accuracy, responseBuffer, percent);
            requestBuffer[sizeof(requestBuffer) - 1] = '\0';

            if(IS_GSM_STATUS_ON())
            {
                const char* serverName = g_ConfigStore.server_addr;
                const char* serverPort = g_ConfigStore.server_port;
                const char* protocol   = g_ConfigStore.server_protocol;
                SSL_Config_t *SSLparam = NULL;
                if (strcmp(protocol, "https") == 0) SSLparam = &SSLconfig;
                else if (strcmp(protocol, "http") != 0) {
                    LOGE("unknown protocol: %s", protocol);
                    continue;
                }

                if (Http_Post(SSLparam, serverName, serverPort, "/", requestBuffer, strlen(requestBuffer), responseBuffer, sizeof(responseBuffer)) < 0)
                    LOGE("FAILED to send the location to the server");
                else
                    LOGI("Sent location to %s", serverName);
            }
            else
            {
                LOGE("No internet");
            }
        }
        else
        {
            LOGE("No GPS fix");
        }

        uint32_t loop_end = time(NULL);
        uint32_t loop_duration = (loop_end - loop_start);
        uint32_t desired_interval = 10; // target loop period in seconds (e.g., 10 seconds)

        if (loop_duration > desired_interval) loop_duration = desired_interval;
        OS_Sleep((desired_interval - loop_duration)*1000);
    }
}

void gps_MainTask(void *pData)
{
    // UART1 to print user logs
    UART_Config_t config = {
        .baudRate = UART_BAUD_RATE_115200,
        .dataBits = UART_DATA_BITS_8,
        .stopBits = UART_STOP_BITS_1,
        .parity   = UART_PARITY_NONE,
        .rxCallback = NULL,
        .useEvent   = true
    };

    TIME_SetIsAutoUpdateRtcTime(true);

    PM_PowerEnable(POWER_TYPE_VPAD,true);
    UART_Init(UART1, config);
    ConfigStore_Init();
    LED_init();
    GPS_Init();    

    OS_CreateTask(gps_trackingTask,
            NULL, NULL, MAIN_TASK_STACK_SIZE, MAIN_TASK_PRIORITY, 0, 0, MAIN_TASK_NAME);

    //Dispatch loop
    while(1)
    {
        API_Event_t* event = NULL;

        if(OS_WaitEvent(gpsTaskHandle, (void**)&event, OS_TIME_OUT_WAIT_FOREVER))
        {
            EventHandler(event);
            OS_Free(event->pParam1);
            OS_Free(event->pParam2);
            OS_Free(event);
        }
    }
}

void app_Main(void)
{
    gpsTaskHandle = OS_CreateTask(gps_MainTask,
        NULL, NULL, MAIN_TASK_STACK_SIZE, MAIN_TASK_PRIORITY, 0, 0, MAIN_TASK_NAME);
    OS_SetUserMainHandle(&gpsTaskHandle);
}
