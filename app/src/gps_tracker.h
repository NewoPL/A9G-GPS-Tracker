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

/**
 * @brief Timestamp of the last tracker loop tick
 * It is set to the timestamp of the last GPS update update loop cycle in gps_trackerTask()
 * it is used to keep even spaces between GPS updates
 * network module uses it to detect if the GPS tracker is stuck on connect function
 * and if so it will try to reactivate GPRS connection
 */ 
extern uint32_t g_trackerloop_tick;

/**
 * @brief Initialize the GPS tracker module.
 * This function sets up the GPS hardware, initializes the internal GPS info structure,
 */
void  gps_Init(void);

/**
 * @brief Check if the GPS data is valid.
 * This function checks the validity of the GPS data by verifying the status
 * reported by the GPS module.
 * @return true if the GPS data is valid, false otherwise
 */
bool  gps_isValid(void);

/**
 * Get the last known GPS latitude.
 * This function retrieves the last valid latitude from the GPS tracker data.
 * @return the latitude in degrees  
 */
float gps_GetLastLatitude(void);

/**
 * @brief Get the last known GPS longitude.
 * This function retrieves the last valid longitude from the GPS tracker data.
 * @return the longitude in degrees
 */
float gps_GetLastLongitude(void);

/**
 * @brief Process the GPS data.
 * This function updates the GpsTrackerData structure with the latest GPS information.
 * It converts NMEA coordinates to decimal degrees and extracts speed, bearing, altitude, and accuracy.
 * it should be called after each call to GPS_Parse().
 */
void  gps_Process(void);

/**
 * @brief Print the current GPS location to the selected output.
 * This function prints the formatted GPS information to UART, Trace, or file depending on the output argument.
 * @param output The log output type (UART, TRACE, or FILE)
 */
void  gps_PrintLocation(t_logOutput output);

/**
 * @brief The main task for the GPS tracker.
 * This function runs in a loop, checking for GPS and GSM status,
 * and sending location updates to the server at specified intervals.
 * It should be started as a separate task in the system.
 * @param pData Pointer to task data (not used)
 */
void  gps_TrackerTask(void *pData);

#endif
