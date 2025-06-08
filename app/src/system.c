#include <stdio.h>
#include <api_os.h>
#include <api_event.h>
#include <api_hal_uart.h>
#include <api_hal_pm.h>

#include "gps.h"
#include "gps_parse.h"

#include "system.h"
#include "network.h"
#include "led_handler.h"
#include "gps_tracker.h"
#include "sms_service.h"
#include "config_store.h"
#include "config_commands.h"
#include "minmea.h"
#include "utils.h"
#include "debug.h"


#define MAIN_TASK_STACK_SIZE      (2048 * 2)
#define MAIN_TASK_PRIORITY        (0)
#define MAIN_TASK_NAME            "GPS Tracker"

#define TRACKER_TASK_STACK_SIZE   (2048 * 2)
#define TRACKER_TASK_PRIORITY     (0)
#define TRACKER_TASK_NAME         "Reporting Task"

HANDLE  trackerTaskHandle = NULL;
HANDLE  appMainTaskHandle = NULL;

uint8_t systemStatus = 0;

uint8_t g_RSSI = 0;
float   g_last_latitude  = 0.0f;
float   g_last_longitude = 0.0f;

static void EventHandler(API_Event_t* pEvent)
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
            gsm_AttachActivate();
            break;

        case API_EVENT_ID_NETWORK_ATTACHED:
            GSM_ACTIVE_OFF(); 
            LOGW("network attach success");
            gsm_AttachActivate();            
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
            networkCellInfoCallback((Network_Location_t*)pEvent->pParam1, pEvent->param1);
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

void app_MainTask(void *pData)
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

    PM_PowerEnable(POWER_TYPE_VPAD, true);
    UART_Init(UART1, config);
    
    UART_Printf("Initialization ...\r\n");
    TIME_SetIsAutoUpdateRtcTime(true);
    apnWorkaround_init();
    ConfigStore_Init();
    FsInfoTest();    
    LED_init();
    LED_BlinkingTimer(appMainTaskHandle);
    networkCellInfoTimer(appMainTaskHandle);
    GPS_Init();
    SMSInit();

    trackerTaskHandle = OS_CreateTask(
        gps_trackerTask, NULL, NULL,
        TRACKER_TASK_STACK_SIZE,
        TRACKER_TASK_PRIORITY, 
        0, 0, TRACKER_TASK_NAME);
    
    // Dispatch loop
    while(true)
    {
        API_Event_t* event = NULL;

        if(OS_WaitEvent(appMainTaskHandle, (void**)&event, OS_TIME_OUT_WAIT_FOREVER))
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
    appMainTaskHandle = OS_CreateTask(
        app_MainTask, NULL, NULL,
        MAIN_TASK_STACK_SIZE, 
        MAIN_TASK_PRIORITY,
        0, 0, MAIN_TASK_NAME);

    OS_SetUserMainHandle(&appMainTaskHandle);
}