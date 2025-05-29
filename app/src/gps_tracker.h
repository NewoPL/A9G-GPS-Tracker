#ifndef GPS_TRACKER_H
#define GPS_TRACKER_H

#define CONFIG_FILE_PATH                 "/config.ini"
#define GPS_NMEA_LOG_FILE_PATH           "/gps_nmea.log"

#define DEFAULT_APN_VALUE                "internet"
#define DEFAULT_APN_PASS_VALUE           ""
#define DEFAULT_APN_USER_VALUE           ""
#define DEFAULT_TRACKING_SERVER_ADDR     "gps.ewane.pl"
#define DEFAULT_TRACKING_SERVER_PORT     "443"
#define DEFAULT_TRACKING_SERVER_PROTOCOL "https"
#define DEFAULT_DEVICE_NAME              "gps"

void UART_Log(const char* fmt, ...);

#endif
