#include <stdio.h>
#include <api_os.h>
#include <api_gps.h>
#include <api_event.h>
#include <api_network.h>
#include <api_hal_pm.h>

#include "system.h"
#include "gps.h"
#include "gps_parse.h"
#include "gps_tracker.h"
#include "sms_service.h"
#include "config_store.h"
#include "config_commands.h"
#include "network.h"
#include "http.h"
#include "utils.h"
#include "debug.h"

uint8_t requestBuffer[400];
uint8_t responseBuffer[1024];

void gps_trackerTask(void *pData)
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
        if(IS_GPS_STATUS_ON()) // && (IS_GPS_FIX()))
        {
            time_t gps_timestamp = mk_time(&gpsInfo->rmc.date, &gpsInfo->rmc.time);
            
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
                     device_name, gpsInfo->rmc.valid, gps_timestamp, g_last_latitude, g_last_longitude, speed, bearing, altitude, accuracy, responseBuffer, percent);
            requestBuffer[sizeof(requestBuffer) - 1] = '\0';

            if(IS_GSM_ACTIVE())
            {
                const char* serverName = g_ConfigStore.server_addr;
                const char* serverPort = g_ConfigStore.server_port;
                bool secure = (g_ConfigStore.server_protocol == PROT_HTTPS);
                if (Http_Post(secure, serverName, serverPort, "/", requestBuffer, strlen(requestBuffer), responseBuffer, sizeof(responseBuffer)) < 0)
                    LOGE("FAILED to send the location to the server");
                else
                    LOGI("Sent location to %s://%s:%s", (secure ? "https":"http"), serverName,serverPort);
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
        OS_Sleep((desired_interval - loop_duration) * 1000);
    }
}
