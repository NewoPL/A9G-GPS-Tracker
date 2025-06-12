#include <api_os.h>
#include <api_gps.h>
#include <api_event.h>
#include <api_hal_pm.h>

#include "system.h"
#include "utils.h"
#include "gps.h"
#include "gps_parse.h"
#include "gps_tracker.h"
#include "config_store.h"
#include "config_commands.h"
#include "network.h"
#include "http.h"
#include "debug.h"

#define MODULE_TAG "GPS"

GPS_Info_t* gpsInfo = NULL;

void gps_Init() 
{
    GPS_STATUS_OFF();
    GPS_Init();
    gpsInfo = Gps_GetInfo();
}

typedef struct {
    time_t timestamp;
    float  latitude;
    float  longitude;
    float  speed;
    float  bearing;
    float  altitude;
    float  accuracy;
} GpsTrackerData_t;

GpsTrackerData_t GpsTrackerData;

void gps_Process(void)
{
    GpsTrackerData.timestamp = mk_time(&gpsInfo->rmc.date, &gpsInfo->rmc.time);

    // Convert NMEA coordinates (DDMM.MMMM format) to decimal degrees
    GpsTrackerData.latitude  = minmea_tocoord(&gpsInfo->rmc.latitude);
    GpsTrackerData.longitude = minmea_tocoord(&gpsInfo->rmc.longitude);
   
    // convert other data a floating point 
    GpsTrackerData.speed     = minmea_tofloat(&gpsInfo->rmc.speed);
    GpsTrackerData.bearing   = minmea_tofloat(&gpsInfo->rmc.course);
    GpsTrackerData.altitude  = minmea_tofloat(&gpsInfo->gga.altitude);
    GpsTrackerData.accuracy  = minmea_tofloat(&gpsInfo->gsa[0].hdop) * 
                                               g_ConfigStore.gps_uere; // User Equivalent Range Error (UERE) in meters 

    if (!gpsInfo->rmc.valid)
        GpsTrackerData.timestamp = time(NULL);

    return;    
}

void gps_PrintLocation(t_logOutput output)
{
    void (*print_func)(const char*, ...) = NULL;
    switch (output) {
        case LOGGER_OUTPUT_UART:
            print_func = UART_Printf;
            break;
        case LOGGER_OUTPUT_FILE:
            print_func = FILE_Printf;
            break;
        case LOGGER_OUTPUT_TRACE:
            Trace(1, "LOGGER_OUTPUT_TRACE is not supported in gps_PrintLocation.\r\n");
            return;
        default:
            LOGE("output type: %d", output);
            return;
    }

    if (!gpsInfo->rmc.valid) {
        print_func("INVALID, ");
    } 
    
    print_func("%02d.%02d.%02d ", gpsInfo->rmc.date.year, gpsInfo->rmc.date.month, gpsInfo->rmc.date.day);
    print_func("%02d.%02d.%02d, ", gpsInfo->rmc.time.hours, gpsInfo->rmc.time.minutes, gpsInfo->rmc.time.seconds);
    
    print_func("sat visble:%d, sat tracked:%d, err: %.1f, ", gpsInfo->gsv[0].total_sats, gpsInfo->gga.satellites_tracked, GpsTrackerData.accuracy);
    print_func("lat: %.6f° %c, lon: %.6f° %c, ", (float)fabs(GpsTrackerData.latitude),  (char)((GpsTrackerData.latitude  >= 0) ? 'N' : 'S'),
              (float)fabs(GpsTrackerData.longitude), (char)((GpsTrackerData.longitude >= 0) ? 'E' : 'W'));
    print_func("alt:%.1f, spd:%.1f, hdg:%.1f\r\n",  GpsTrackerData.altitude, GpsTrackerData.speed, GpsTrackerData.bearing);
    return;
}

float gps_GetLastLatitude(void)
{
    return GpsTrackerData.latitude;
}

float gps_GetLastLongitude(void)
{
    return GpsTrackerData.longitude;
}

bool  gps_isValid(void) 
{
    return gpsInfo->rmc.valid && 
           (gpsInfo->rmc.latitude.scale != 0) && 
           (gpsInfo->rmc.longitude.scale != 0);
}

uint32_t g_trackerloop_tick = 0;
uint8_t  requestBuffer[400];
uint8_t  responseBuffer[1024];

void gps_TrackerTask(void *pData)
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
   
    RTC_Time_t time;
    TIME_GetRtcTime(&time);
    if(!GPS_SetRtcTime(&time)) LOGE("set gps time failed");

    GPS_SetSearchMode(true, false, true, true);

    // if(!GPS_ClearLog())
    //    LOGE("open file failed, please check tf card");

    //if(!GPS_ClearInfoInFlash())
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

    // Target loop period in seconds. It is set to:
    // 10s when sending data to server 
    // 1s when waiting for the internet connection or a GPS fix
    uint32_t desired_interval = 0;
    
    while(1)
    {
        g_trackerloop_tick = time(NULL);

        if(IS_GPS_STATUS_ON() && IS_GSM_ACTIVE())
        {
            uint8_t percent;
            PM_Voltage(&percent);

            gps_PrintLocation(g_ConfigStore.logOutput);

            responseBuffer[0] = '\0';
            const char* cellInfoStr = Network_GetCellInfoString();
            if (cellInfoStr && strlen(cellInfoStr) != 0)
                snprintf(responseBuffer, sizeof(responseBuffer),"&cell=%s", cellInfoStr);

            snprintf(requestBuffer, sizeof(requestBuffer),
                "id=%s&valid=%d&timestamp=%d&lat=%f&lon=%f&speed=%1.f&bearing=%.1f&altitude=%.1f&accuracy=%.1f%s&batt=%d",
                g_ConfigStore.device_name, gpsInfo->rmc.valid, 
                GpsTrackerData.timestamp, GpsTrackerData.latitude, GpsTrackerData.longitude, 
                GpsTrackerData.speed,     GpsTrackerData.bearing,  GpsTrackerData.altitude, 
                GpsTrackerData.accuracy, responseBuffer, percent);
            requestBuffer[sizeof(requestBuffer) - 1] = '\0';

            const char* serverName = g_ConfigStore.server_addr;
            const char* serverPort = g_ConfigStore.server_port;
            const bool  secure = (g_ConfigStore.server_protocol == PROT_HTTPS);
            int result = Http_Post(secure, serverName, serverPort, "/", 
                                   requestBuffer, strlen(requestBuffer),
                                   responseBuffer, sizeof(responseBuffer));
            if (result < 0)
                LOGE("FAILED to send the location to the server. err: %d", result);
            else
                LOGI("Sent location to %s://%s:%s", (secure ? "https":"http"), serverName, serverPort);
            
            // wait 10 seconds before next loop iteration
            desired_interval = 10; 
        }
        else
        {
            // if there is no internet do not wait too long for the next loop
            desired_interval = 1;
        }

        uint32_t loop_duration = (time(NULL) - g_trackerloop_tick);
        if (loop_duration > desired_interval) loop_duration = desired_interval;
        OS_Sleep((desired_interval - loop_duration) * 1000);
    }
}

// Returns 0 on success, nonzero on failure
int gps_PerformAgps(void)
{
    float latitude = 0.0, longitude = 0.0;
    int lbs_ok = Network_GetLbsLocation(&longitude, &latitude);
    if (!lbs_ok) {
        UART_Printf("LBS get location failed.\r\n");
        longitude = gps_GetLastLongitude();
        latitude = gps_GetLastLatitude();
        UART_Printf("Last known position: lat: %.6f, lon: %.6f\r\n",
                    latitude, longitude);
    } else {
        UART_Printf("Network based position: lat: %.6f, lon: %.6f\r\n",
                    latitude, longitude);
    }
    // Call AGPS
    if (!GPS_AGPS(latitude, longitude, 0, true)) {
        UART_Printf("Assisted GPS start failed\n\r");
        return -1; // AGPS failed
    }
    UART_Printf("Assisted GPS start succeeded\n\r");
    return 0; // AGPS success
}
