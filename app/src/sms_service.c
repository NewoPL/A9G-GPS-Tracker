#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "api_os.h"
#include "api_debug.h"
#include "api_event.h"
#include "api_sms.h"
#include "api_hal_uart.h"
#include "gps_tracker.h"
#include "config_store.h"
#include "minmea.h"
#include "utils.h"
#include "debug.h"

// Forward declaration for sending location SMS
static void SendLocationSms(const char* phoneNumber);

// Helper to get last known position as Google Maps link
static bool GetGoogleMapsLink(char* buf, size_t bufsize) {
    // You may want to use actual last known position from your GPS logic
    // For now, use global config or a stub
    float lat = 0.0f, lon = 0.0f;
    // TODO: Replace with actual retrieval of last known position
    extern float g_last_latitude;
    extern float g_last_longitude;
    lat = g_last_latitude;
    lon = g_last_longitude;
    if (lat == 0.0f && lon == 0.0f) {
        snprintf(buf, bufsize, "Location unknown");
        return false;
    }
    snprintf(buf, bufsize, "https://maps.google.com/?q=%f,%f", lat, lon);
    return true;
}

void HandleSmsReceived(API_Event_t* pEvent) {
    SMS_Encode_Type_t encodeType = pEvent->param1;
    uint32_t contentLength = pEvent->param2;
    uint8_t* header = pEvent->pParam1;
    uint8_t* content = pEvent->pParam2;
    char phoneNumber[32] = {0};
    // Parse phone number from header (assume header is ASCII and contains number)
    strncpy(phoneNumber, (const char*)header, sizeof(phoneNumber)-1);
    LOGI( "SMS received from: %s, encodeType: %d, contentLength: %d", phoneNumber, encodeType, contentLength);
    // Only handle ASCII for command parsing
    if (encodeType == SMS_ENCODE_TYPE_ASCII) {
        char cmd[64] = {0};
        strncpy(cmd, (const char*)content, contentLength < sizeof(cmd)-1 ? contentLength : sizeof(cmd)-1);
        trim_whitespace(cmd);
        LOGI("SMS content: '%s'", cmd);
        if (str_case_cmp(cmd, "get location") == 0) {
            LOGI("Recognized 'get location' command from %s", phoneNumber);
            SendLocationSms(phoneNumber);
        } else {
            LOGI("Unknown SMS command received: '%s'", cmd);
        }
    } else {
        LOGI("SMS received with unsupported encoding type: %d", encodeType);
    }
}

static void SendLocationSms(const char* phoneNumber) {
    char msg[128];
    if (GetGoogleMapsLink(msg, sizeof(msg))) {
        LOGI("Sending location SMS to %s: %s", phoneNumber, msg);
        SMS_SendMessage(phoneNumber, (const uint8_t*)msg, strlen(msg), SIM0);
    } else {
        const char* err = "Location not available";
        LOGI("Sending error SMS to %s: %s", phoneNumber, err);
        SMS_SendMessage(phoneNumber, (const uint8_t*)err, strlen(err), SIM0);
    }
}
