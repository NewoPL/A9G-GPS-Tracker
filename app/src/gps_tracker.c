#include <string.h>
#include <stdio.h>
#include <api_os.h>
#include <api_gps.h>
#include <api_info.h>
#include <api_event.h>
#include <api_network.h>
#include <api_lbs.h>
#include <api_hal_uart.h>
#include <api_hal_pm.h>
#include <api_debug.h>

#include "buffer.h"
#include "gps_parse.h"
#include "math.h"
#include "gps.h"
#include "time.h"
#include "assert.h"

#include "system.h"
#include "gps_tracker.h"
#include "network.h"
#include "config.h"
#include "config_parser.h"
#include "led_handler.h"

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

// const uint8_t nmea[]="$GNGGA,000021.263,2228.7216,N,11345.5625,E,0,0,,153.3,M,-3.3,M,,*4E\r\n$GPGSA,A,1,,,,,,,,,,,,,,,*1E\r\n$BDGSA,A,1,,,,,,,,,,,,,,,*0F\r\n$GPGSV,1,1,00*79\r\n$BDGSV,1,1,00*68\r\n$GNRMC,000021.263,V,2228.7216,N,11345.5625,E,0.000,0.00,060180,,,N*5D\r\n$GNVTG,0.00,T,,M,0.000,N,0.000,K,N*2C\r\n";

void UART_Log(const char* fmt, ...) 
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (len > 0) {
        // Clamp to max buffer size
        if ((size_t)len >= sizeof(buffer)) {
            len = sizeof(buffer) - 1;  
        }
        UART_Write(UART1, buffer, (size_t)len);
    }
    
    return;
}

bool AttachActivate()
{
    uint8_t status;
    bool ret = Network_GetAttachStatus(&status);
    if(!ret)
    {
        UART_Log("ERROR - get attach status\r\n");
        return false;
    }
    if(!status)
    {
        ret = Network_StartAttach();
        UART_Log("attaching to the network\r\n");
        if(!ret)
        {
            UART_Log("ERROR - network attach failed\r\n");
            return false;
        }
    }
    else
    {
        ret = Network_GetActiveStatus(&status);
        if(!ret)
        {
            UART_Log("ERROR - get activate status failed\r\n");
            return false;
        }
        if(!status)
        {
            /*
            Network_PDP_Context_t NetContextDummy = {
                .apn = "dummy",
                .userName = "dummy",
                .userPasswd = "dummy"
            };
            ret = Network_StartActive(NetContextDummy);
            if(!ret) {
               UART_Log("ERROR - network activate failed fail\r\n");
               return false;
            }
            */
            UART_Log("activating the network\r\n");
            Config_GetValue(&ConfigStore, KEY_APN,      NetContext.apn,        sizeof(NetContext.apn));
            Config_GetValue(&ConfigStore, KEY_APN_USER, NetContext.userName,   sizeof(NetContext.userName));
            Config_GetValue(&ConfigStore, KEY_APN_PASS, NetContext.userPasswd, sizeof(NetContext.userPasswd));
            ret = Network_StartActive(NetContext);
            if(!ret) {
               UART_Log("ERROR - network activate failed fail\r\n");
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

void EventDispatch(API_Event_t* pEvent)
{
    switch(pEvent->id)
    {
        case API_EVENT_ID_NO_SIMCARD:
            GSM_STATUS_OFF();
            UART_Log("ERROR - no sim card %d !\r\n", pEvent->param1);
            break;
        case API_EVENT_ID_SIMCARD_DROP:
            GSM_STATUS_OFF();
            UART_Log("ERROR - sim card %d drop !\r\n",pEvent->param1);
            break;
        case API_EVENT_ID_NETWORK_REGISTER_SEARCHING:
            UART_Log("network register searching\r\n");
            break;
        case API_EVENT_ID_NETWORK_REGISTER_DENIED:
            UART_Log("network register denied\r\n");
            GSM_STATUS_OFF();
            break;
        case API_EVENT_ID_NETWORK_REGISTER_NO:
            GSM_STATUS_OFF();
            UART_Log("network register no\r\n");
            break;

        case API_EVENT_ID_NETWORK_REGISTERED_HOME:
        case API_EVENT_ID_NETWORK_REGISTERED_ROAMING:
            UART_Log("network register success\r\n");
            AttachActivate();
            break;

        case API_EVENT_ID_NETWORK_ATTACHED:
            UART_Log("network attach success\r\n");
            AttachActivate();
            break;

        case API_EVENT_ID_NETWORK_ACTIVATED:
            GSM_STATUS_ON();
            UART_Log("network activate success\r\n");
            break;

        case API_EVENT_ID_NETWORK_ACTIVATE_FAILED:
            GSM_STATUS_OFF();
            UART_Log("network activate failed\r\n");
            break;

        case API_EVENT_ID_NETWORK_DEACTIVED:
            GSM_STATUS_OFF(); 
            UART_Log("network deactived\r\n");
            AttachActivate();
            break;

        case API_EVENT_ID_NETWORK_ATTACH_FAILED:
            GSM_STATUS_OFF();
            UART_Log("network attach failed\r\n");
            break;

        case API_EVENT_ID_NETWORK_DETACHED:
            GSM_STATUS_OFF();
            UART_Log("network detached\r\n");
            AttachActivate();
            break;

        case API_EVENT_ID_SIGNAL_QUALITY:
            g_RSSI = csq_to_percent(pEvent->param1);
            UART_Log("CSQ: %d %\r\n", g_RSSI);
            break;

        case API_EVENT_ID_NETWORK_CELL_INFO:
            processNetworkCellInfo((Network_Location_t*)pEvent->pParam1, pEvent->param1);
            break;

        case API_EVENT_ID_SYSTEM_READY:
            UART_Log("system initialize complete\r\n");
            INITIALIZED_ON();
            break;

        case API_EVENT_ID_GPS_UART_RECEIVED:
            // Trace(1,"received GPS data,length:%d, data:%s,flag:%d",pEvent->param1,pEvent->pParam1,flag);
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
    // The process of GPRS registration network may cause the power supply voltage of GPS to drop, which resulting in GPS restart.
    UART_Log("Initialization ");
    LED_cycle_start(gpsTaskHandle);

    while(!IS_INITIALIZED() || !IS_GSM_STATUS_ON())
    {
        UART_Log(".");
        OS_Sleep(2000);
    }
    UART_Log("\r\n");

    GPS_Info_t* gpsInfo = Gps_GetInfo();
    GPS_SaveLog(false, GPS_NMEA_LOG_FILE_PATH);

    // open GPS hardware(UART2 open either)
    GPS_Open(NULL);

    UART_Log("Waiting for GPS ");
    while(!IS_GPS_STATUS_ON())
    {
        UART_Log(".");
        OS_Sleep(2000);
    }
    UART_Log("\r\n");

    if(!GPS_GetVersion(responseBuffer, 255))
        UART_Log("ERROR - get GPS firmware version fail.\r\n");
    else
        UART_Log("GPS firmware version: %s\r\n", responseBuffer);

//    if(!GPS_SetSearchMode(true, true, false, true))
        //UART_Log("ERROR - set search mode fail.\r\n");

    if(!GPS_SetFixMode(GPS_FIX_MODE_NORMAL))
        UART_Log("ERROR - set fix mode fail.\r\n");

    // if(!GPS_ClearLog())
    //     UART_Log("open file error, please check tf card\r\n");

    // if(!GPS_ClearInfoInFlash())
    //     UART_Log("erase gps fail\r\n");
    
    // if(!GPS_SetQzssOutput(false))
    //     UART_Log("enable qzss nmea output fail\r\n");


    // if(!GPS_SetSBASEnable(true))
    //     UART_Log("enable sbas fail\r\n");
    
    UART_Log("setting GPS LP Mode to GPS_LP_MODE_NORMAL.\r\n");
    if(!GPS_SetLpMode(GPS_LP_MODE_SUPPER_LP))
        UART_Log("ERROR - set GPS LP mode failed.\r\n");

    UART_Log("setting GPS interval to 1000 ms\r\n");
    if(!GPS_SetOutputInterval(1000))
        UART_Log("ERROR - set GPS interval failed.\r\n");
    
    const char *device_name = Config_GetValue(&ConfigStore, KEY_DEVICE_NAME, NULL, 0);
    UART_Log("Device name: %s\r\n", device_name);

    while(1)
    {
        uint32_t loop_start = time(NULL);
        if(IS_GPS_STATUS_ON() && (gpsInfo->rmc.valid))
        {
            if(!Network_GetCellInfoRequst()) {
                g_cellInfo[0] = '\0';
                UART_Log("ERROR - network get cell info fail\r\n");
            }

            struct timespec timestamp;
            minmea_gettime(&timestamp, &gpsInfo->rmc.date, &gpsInfo->rmc.time);

            // convert unit ddmm.mmmm to a floating point DD.DDD vale in degree(°) 
            float latitude  = minmea_tocoord(&gpsInfo->rmc.latitude);
            float longitude = minmea_tocoord(&gpsInfo->rmc.longitude);
            float speed     = minmea_tofloat(&gpsInfo->rmc.speed);
            float bearing   = minmea_tofloat(&gpsInfo->rmc.course);
            float altitude  = minmea_tofloat(&gpsInfo->gga.altitude);
            //float accuracy  = sqrt(pow(minmea_tofloat(&gpsInfo->gst.latitude_error_deviation), 2) +
            //                       pow(minmea_tofloat(&gpsInfo->gst.longitude_error_deviation), 2));
            float accuracy  = 10;
            
            uint8_t percent;
            PM_Voltage(&percent);

            snprintf(responseBuffer, sizeof(responseBuffer),"time=%d, timestamp:%d, GPS fix mode:%d, BDS fix mode:%d, "
                                                            "fix quality:%d, satellites tracked:%d, gps sates total:%d, "
                                                            "valid: %d, Latitude:%f, Longitude:%f, altitude:%f, "
                                                            "accuaracy = %.1f, "
                                                            "speed= %.1f, "
                                                            "course= %.1f, " 
                                                            "battery= %d\r\n",
                                                            time(NULL), timestamp.tv_sec, gpsInfo->gsa[0].fix_type, gpsInfo->gsa[1].fix_type,
                                                            gpsInfo->gga.fix_quality, gpsInfo->gga.satellites_tracked, gpsInfo->gsv[0].total_sats, 
                                                            gpsInfo->rmc.valid, latitude, longitude, altitude, accuracy, speed, bearing, percent);
            responseBuffer[sizeof(responseBuffer) - 1] = '\0';

            //send to UART1
            UART_Write(UART1, responseBuffer, strlen(responseBuffer));

            responseBuffer[0] = '\0';
            if (strlen(g_cellInfo) != 0) {
                 snprintf(responseBuffer, sizeof(responseBuffer),"&cell=%s", g_cellInfo);
            }

            snprintf(requestBuffer, sizeof(requestBuffer),
                     "id=%s&valid=%d&timestamp=%d&lat=%f&lon=%f&speed=%1.f&bearing=%.1f&altitude=%.1f&accuracy=%.1f%s&batt=%d",
                     device_name, gpsInfo->rmc.valid, time(NULL), latitude, longitude, speed, bearing, altitude, accuracy, responseBuffer, percent);
            requestBuffer[sizeof(requestBuffer) - 1] = '\0';

            if(IS_GSM_STATUS_ON())
            {
                const char* serverName = Config_GetValue(&ConfigStore, KEY_TRACKING_SERVER_ADDR,     NULL, 0);
                const char* serverPort = Config_GetValue(&ConfigStore, KEY_TRACKING_SERVER_PORT,     NULL, 0);
                const char* protocol   = Config_GetValue(&ConfigStore, KEY_TRACKING_SERVER_PROTOCOL, NULL, 0);
                if (strcmp(protocol, "https") == 0)
                {
                    if (Https_Post(&SSLconfig, serverName, serverPort, "/", requestBuffer, strlen(requestBuffer), responseBuffer, sizeof(responseBuffer)) < 0)
                        UART_Log("FAILED to send the location to the server via HTTPS\r\n");
                    else
                        UART_Log("sent location to server via HTTPS\r\n");
                }
                else if (strcmp(protocol, "http") == 0)
                {
                    if (Http_Post(serverName, atoi(serverPort), "/", requestBuffer, strlen(requestBuffer), responseBuffer, sizeof(responseBuffer)) < 0)
                        UART_Log("FAILED to send the location to the server via HTTP\r\n");
                    else
                        UART_Log("sent location to server via HTTP\r\n");
                }
                else
                {
                    UART_Log("unknown protocol: %s\r\n", protocol);
                }
            }
            else
            {
                UART_Log("no internet\r\n");
            }
        }
        else
        {
            UART_Log("no GPS fix\r\n");
        }

        uint32_t loop_end = time(NULL);
        uint32_t loop_duration = (loop_end - loop_start);
        uint32_t desired_interval = 10; // target loop period in ms (e.g., 10 seconds)

        if (loop_duration < desired_interval) 
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
            EventDispatch(event);
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
