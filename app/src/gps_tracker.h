#ifndef GPS_TRACKER_H
#define GPS_TRACKER_H

#define CONFIG_FILE_PATH          "/config.ini"
#define GPS_NMEA_LOG_FILE_PATH    "/t/gps_nmea.log"

#define DEFAULT_APN_VALUE         "internet"
#define DEFAULT_APN_PASS_VALUE    ""
#define DEFAULT_APN_USER_VALUE    ""
#define DEFAULT_SERVER_ADDR       "demo3.traccar.org"
#define DEFAULT_SERVER_PORT       "5055"
#define DEFAULT_SERVER_PROTOCOL   "https"
#define DEFAULT_DEVICE_NAME       "IMEI"
#define DEFAULT_GPS_UERE          "5"
#define DEFAULT_GPS_LOGS          "disabled"
#define DEFAULT_LOG_LEVEL         "info"
#define DEFAULT_LOG_OUTPUT        "uart"

extern uint32_t g_trackerloop_tick;

void  gps_Init(void);

bool  gps_isValid(void);

// Get the last known GPS coordinates
// Returns the latitude in degrees  
float gps_GetLastLatitude(void);

// Returns the longitude in degrees
float gps_GetLastLongitude(void);

void  gps_Process(void);

void  gps_PrintLocation(void);

void  gps_trackerTask(void *pData);

#endif
