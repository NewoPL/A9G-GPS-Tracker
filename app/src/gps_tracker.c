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
#include "sms_service.h"

#include "system.h"
#include "gps_tracker.h"
#include "config_store.h"
#include "config_commands.h"
#include "led_handler.h"
#include "network.h"
#include "utils.h"
#include "debug.h"

#define MAIN_TASK_STACK_SIZE      (2048 * 2)
#define MAIN_TASK_PRIORITY        (0)
#define MAIN_TASK_NAME            "GPS Tracker"

#define REPORTING_TASK_STACK_SIZE (2048 * 2)
#define REPORTING_TASK_PRIORITY   (0)
#define REPORTING_TASK_NAME       "Reporting Task"

HANDLE gpsMainTaskHandle   = NULL;
HANDLE reportingTaskHandle = NULL;

uint8_t systemStatus = 0;
uint8_t g_RSSI = 0;
char    g_cellInfo[128] = "\0";
// Provide last known position for SMS service
float   g_last_latitude = 0.0f;
float   g_last_longitude = 0.0f;


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


// --- APN Re-activation Workaround ---
//
// the firmware has a defect where re-activating the same APN after deactivation fails.
// To work around this, we:
//   1. Attempt to activate a dummy/incorrect APN first (NetContextArr[1]).
//   2. Wait for a network deactivation event (which occurs because the dummy APN fails).
//   3. Then activate the real APN (NetContextArr[0]).
//
// This workaround is managed by the apn_workaround_pending flag and the AttachActivate() function.
// The dummy APN context is set once at system initialization. The real APN context is set as needed.
// All logic for the workaround is centralized in AttachActivate; 
// -----------------------------------

Network_PDP_Context_t NetContextArr[2];      // Two-element array: [0]=real APN, [1]=dummy APN
static bool apn_workaround_pending = false;  

static void apnWorkaround_init(void) 
{
    // set Workaround state for APN re-activation
    apn_workaround_pending = false;
    // Dummy APN (index 1) context is set once at system init
    memset(&NetContextArr[1], 0, sizeof(NetContextArr[1]));
    strncpy(NetContextArr[1].apn, "dummy_apn", sizeof(NetContextArr[1].apn)-1);
}

// Helper to initialize the real APN context
static void SetApnContext() {
    // Real APN (index 0)
    memset(&NetContextArr[0], 0, sizeof(NetContextArr[0]));
    strncpy(NetContextArr[0].apn, g_ConfigStore.apn, sizeof(NetContextArr[0].apn)-1);
    strncpy(NetContextArr[0].userName, g_ConfigStore.apn_user, sizeof(NetContextArr[0].userName)-1);
    strncpy(NetContextArr[0].userPasswd, g_ConfigStore.apn_pass, sizeof(NetContextArr[0].userPasswd)-1);
}

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

            // Implements the APN re-activation workaround:
            // - If workaround is pending, activate the real APN (NetContextArr[0]) and clear the flag.
            // - Otherwise, activate the dummy APN (NetContextArr[1]) and set the flag.
            // This ensures the modem can recover from the re-activation defect.
            if (apn_workaround_pending) {
                SetApnContext();
                ret = Network_StartActive(NetContextArr[0]); // real APN
                apn_workaround_pending = false;
            } else {
                LOGW("Trying to activate dummy APN to workaround re-activation defect");
                apn_workaround_pending = true;
                ret = Network_StartActive(NetContextArr[1]); // dummy
            }
            if (!ret)
                LOGE("Failed to activate APN");
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
        return;
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
            GSM_ACTIVE_OFF();
            LOGE("no sim card %d !", pEvent->param1);
            break;
        case API_EVENT_ID_SIMCARD_DROP:
            GSM_ACTIVE_OFF();
            LOGE("sim card %d drop !",pEvent->param1);
            break;
        case API_EVENT_ID_NETWORK_REGISTER_DENIED:
            GSM_REGISTERED_OFF();
            LOGE("network register denied");
            break;
        case API_EVENT_ID_NETWORK_REGISTER_NO:
            GSM_REGISTERED_OFF();
            LOGE("network register no");
            break;

        case API_EVENT_ID_NETWORK_REGISTERED_HOME:
        case API_EVENT_ID_NETWORK_REGISTERED_ROAMING:
            GSM_REGISTERED_ON();    
            LOGW("network register success");
            AttachActivate();
            break;

        case API_EVENT_ID_NETWORK_ATTACHED:
            GSM_ACTIVE_OFF(); 
            LOGW("network attach success");
            break;

        case API_EVENT_ID_NETWORK_ACTIVATED:
            GSM_ACTIVE_ON();
            LOGW("network activate success");
            break;

        case API_EVENT_ID_NETWORK_ACTIVATE_FAILED:
            LOGE("network activate failed");
            break;

        case API_EVENT_ID_NETWORK_DEACTIVED:
            GSM_ACTIVE_OFF(); 
            LOGE("network deactived");
            break;

        case API_EVENT_ID_NETWORK_ATTACH_FAILED:
            GSM_ACTIVE_OFF(); 
            LOGE("network attach failed");
            break;

        case API_EVENT_ID_NETWORK_DETACHED:
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

        case API_EVENT_ID_SMS_RECEIVED:
            HandleSmsReceived(pEvent);
            break;

        case API_EVENT_ID_GPS_UART_RECEIVED:
            if (g_ConfigStore.gps_logging)
                LOGD("received GPS data, length:%d, data:\r\n%s",pEvent->param1,pEvent->pParam1);

            GPS_STATUS_ON();
            GPS_Update(pEvent->pParam1, pEvent->param1);

            GPS_Info_t* gpsInfo = Gps_GetInfo();
            if (gpsInfo->rmc.valid) {
                GPS_FIX_ON();
                g_last_latitude = minmea_tocoord(&gpsInfo->rmc.latitude);
                g_last_longitude = minmea_tocoord(&gpsInfo->rmc.longitude);
            } else {
                GPS_FIX_OFF();
            }

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
    while (!IS_INITIALIZED() || !IS_GSM_ACTIVE()) OS_Sleep(2000);

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
        GPS_Info_t* gpsInfo = Gps_GetInfo();
        uint32_t    loop_start = time(NULL);
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

            /*
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
            */

            responseBuffer[0] = '\0';
            if (strlen(g_cellInfo) != 0) {
                 snprintf(responseBuffer, sizeof(responseBuffer),"&cell=%s", g_cellInfo);
            }

            snprintf(requestBuffer, sizeof(requestBuffer),
                     "id=%s&valid=%d&timestamp=%d&lat=%f&lon=%f&speed=%1.f&bearing=%.1f&altitude=%.1f&accuracy=%.1f%s&batt=%d",
                     device_name, gpsInfo->rmc.valid, gps_timestamp, latitude, longitude, speed, bearing, altitude, accuracy, responseBuffer, percent);
            requestBuffer[sizeof(requestBuffer) - 1] = '\0';

            if(IS_GSM_ACTIVE())
            {
                const char* serverName = g_ConfigStore.server_addr;
                const char* serverPort = g_ConfigStore.server_port;
                t_protocol  protocol   = g_ConfigStore.server_protocol;
                SSL_Config_t *SSLparam = NULL;
                if (protocol == PROT_HTTPS) SSLparam = &SSLconfig;
                else if (protocol != PROT_HTTP) {
                    LOGE("unknown protocol: %d", protocol);
                    continue;
                }

                if (Http_Post(SSLparam, serverName, serverPort, "/", requestBuffer, strlen(requestBuffer), responseBuffer, sizeof(responseBuffer)) < 0)
                    LOGE("FAILED to send the location to the server");
                else
                    LOGE("Sent location to %s", serverName);
            }
            else
            {
                LOGE("No internet registered:%d, active:%d", 
                     IS_GSM_REGISTERED(), IS_GSM_ACTIVE());           }
        }
        else
        {
            LOGE("No GPS fix. SAT visible: %d, SAT tracked:%d", 
                 gpsInfo->gsv[0].total_sats, gpsInfo->gga.satellites_tracked);
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
    
    UART_Printf("Initialization ...\r\n");
    apnWorkaround_init();
    ConfigStore_Init();
    FsInfoTest();    
    LED_init();
    GPS_Init();    
    LED_cycle_start(gpsMainTaskHandle);

    reportingTaskHandle = OS_CreateTask(
        gps_trackingTask, NULL, NULL,
        REPORTING_TASK_STACK_SIZE,
        REPORTING_TASK_PRIORITY, 
        0, 0, REPORTING_TASK_NAME);
    
    //Dispatch loop
    while(1)
    {
        API_Event_t* event = NULL;

        if(OS_WaitEvent(gpsMainTaskHandle, (void**)&event, OS_TIME_OUT_WAIT_FOREVER))
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
    gpsMainTaskHandle = OS_CreateTask(
        gps_MainTask, NULL, NULL,
        MAIN_TASK_STACK_SIZE, 
        MAIN_TASK_PRIORITY,
        0, 0, MAIN_TASK_NAME);

    OS_SetUserMainHandle(&gpsMainTaskHandle);
}
