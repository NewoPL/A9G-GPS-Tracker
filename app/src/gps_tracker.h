#ifndef GPS_TRACKER_H
#define GPS_TRACKER_H

#define CONFIG_FILE_PATH          "/config.ini"
#define GPS_NMEA_LOG_FILE_PATH    "/t/gps_nmea.log"

#define DEFAULT_APN_VALUE         "internet"
#define DEFAULT_APN_PASS_VALUE    ""
#define DEFAULT_APN_USER_VALUE    ""
#define DEFAULT_SERVER_ADDR       "gps.ewane.pl"
#define DEFAULT_SERVER_PORT       "443"
#define DEFAULT_SERVER_PROTOCOL   "https"
#define DEFAULT_DEVICE_NAME       ""
#define DEFAULT_GPS_UERE          "5"
#define DEFAULT_GPS_LOGS          "disabled"
#define DEFAULT_LOG_LEVEL         "info"
#define DEFAULT_LOG_OUTPUT        "uart"

void apnWorkaround_init(void);
bool gsm_AttachActivate(void);
void gps_trackerTask(void *pData);

#endif
