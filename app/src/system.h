#ifndef SYSTEM_H
#define SYSTEM_H

/**
 * Interval in milliseconds for network monitoring
 * This is used to check the network status periodically.
 * Network monitor asks for the current cell information and verifies
 * if the gps tracker thread is not stuck on initiating connection.
 * in case of ssl connection failure, the network is reactivated.
 */
#define NETWORK_MONITOR_INTERVAL_MS  15000 

typedef enum {
    STATUS_INITIALIZED    = 1 << 0,
    STATUS_GPS_ON         = 1 << 1,
    STATUS_GSM_ACTIVE     = 1 << 2,
    STATUS_GSM_REGISTERED = 1 << 3,
    STATUS_SLEEPING       = 1 << 4
} StatusFlags;

// Initialized
#define INITIALIZED_ON()   (systemStatus |= STATUS_INITIALIZED)
#define INITIALIZED_OFF()  (systemStatus &= ~STATUS_INITIALIZED)
#define IS_INITIALIZED()   (systemStatus & STATUS_INITIALIZED)

// GPS
#define GPS_STATUS_ON()    (systemStatus |= STATUS_GPS_ON)
#define GPS_STATUS_OFF()   (systemStatus &= ~STATUS_GPS_ON)
#define IS_GPS_STATUS_ON() (systemStatus & STATUS_GPS_ON)

// GSM Active
#define GSM_ACTIVE_ON()    (systemStatus |= STATUS_GSM_ACTIVE)
#define GSM_ACTIVE_OFF()   (systemStatus &= ~STATUS_GSM_ACTIVE)
#define IS_GSM_ACTIVE()    (systemStatus & STATUS_GSM_ACTIVE)

// GSM Registered
#define GSM_REGISTERED_ON()   (systemStatus |= STATUS_GSM_REGISTERED)
#define GSM_REGISTERED_OFF()  (systemStatus &= ~STATUS_GSM_REGISTERED)
#define IS_GSM_REGISTERED()   (systemStatus & STATUS_GSM_REGISTERED)

// Sleeping
#define SLEEPING_ON()      (systemStatus |= STATUS_SLEEPING)
#define SLEEPING_OFF()     (systemStatus &= ~STATUS_SLEEPING)
#define IS_SLEEPING()      (systemStatus & STATUS_SLEEPING)

// The actual bitfield variable
extern uint8_t systemStatus;

#endif
