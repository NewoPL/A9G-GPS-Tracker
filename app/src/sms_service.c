#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <api_os.h>
#include <api_debug.h>
#include <api_event.h>
#include <api_sms.h>
#include <api_hal_uart.h>

#include "system.h"
#include "gps_parse.h"
#include "gps_tracker.h"
#include "config_store.h"
#include "sms_service.h"
#include "minmea.h"
#include "utils.h"
#include "debug.h"

void SMSInit()
{
    if(!SMS_SetFormat(SMS_FORMAT_TEXT, SIM0))
    {
        LOGE("sms set format error");
        return;
    }

    SMS_Parameter_t smsParam = {
        .fo = 17 , // Format options, 17 means SMS format is Text
        .vp = 11,  // Validity period, 2 hours
        .pid= 0  , // Protocol Identifier, 0 means no specific protocol
        .dcs= 4  , // English 8-bit data (can include some special characters) - up to 140 characters
    };
    
    if(!SMS_SetParameter(&smsParam, SIM0))
    {
        LOGE("sms set parameter error");
        return;
    }
    
    if(!SMS_SetNewMessageStorage(SMS_STORAGE_SIM_CARD))
    {
        LOGE("sms set message storage fail");
        return;
    }
}

// Helper to get last known position as Google Maps link
static bool GetGoogleMapsLink(char* buf, size_t bufsize)
{
    float latitude = gps_GetLastLatitude();
    float longitude = gps_GetLastLongitude();
    if (latitude == 0.0f && longitude == 0.0f)
        return false;
    
    // Format with proper precision for Google Maps link
    snprintf(buf, bufsize, "https://maps.google.com/?q=%.6f,%.6f",
             latitude, longitude);
    return true;
}

void HandleSmsReceived(API_Event_t* pEvent)
{
    SMS_Encode_Type_t encodeType = pEvent->param1;
    const char* headerStr = pEvent->pParam1;
    const char* content = pEvent->pParam2; 
    uint32_t contentLength = pEvent->param2;    

    char cmd[SMS_BODY_MAX_LEN] = {0};

    // Process content based on encoding type
    if (encodeType == SMS_ENCODE_TYPE_ASCII) {
        // Direct copy for ASCII encoding
        strncpy(cmd, (const char*)content, contentLength < sizeof(cmd)-1 ? contentLength : sizeof(cmd)-1);
    } 
    else if (encodeType == SMS_ENCODE_TYPE_UNICODE) {
        // Extract text from Unicode (UCS-2) encoding
        // In Unicode, each character is 2 bytes, with ASCII characters having 0x00 as the high byte
        uint16_t cmdLen = 0;
        for (uint16_t i = 0; i < contentLength && cmdLen < sizeof(cmd)-1; i += 2) {
            // Check if it's ASCII character in Unicode format (high byte is 0)
            if (i+1 < contentLength && content[i] == 0) {
                cmd[cmdLen++] = content[i+1];
            }
        }
    }
    else {
        LOGE("SMS received with unsupported encoding type: %d", encodeType);
        return;
    }
    
    trim_whitespace(cmd);
    LOGI("SMS received header: %s, encodeType: %d, contentLength: %d", headerStr, encodeType, contentLength);
    LOGI("SMS content (processed): '%s'", cmd);
 
    // Parse phone number from header - it's enclosed in quotes and followed by a comma
    // Format example: "+1234567890","2023/06/08,11:22:33+00"

    // Direct character-by-character parsing approach
    int phoneIdx = 0;
    bool insideQuotes = false;
    char phoneNumber[SMS_PHONE_NUMBER_MAX_LEN+1] = {0};

    for (int i = 0; headerStr[i] != '\0' && phoneIdx < sizeof(phoneNumber) - 1; i++) {
        char c = headerStr[i];
        
        // Start collecting after finding the opening quote
        if (!insideQuotes && c == '\"') {
            insideQuotes = true;
            continue;
        }
        
        // Stop when we hit closing quote
        if (insideQuotes && c == '\"') {
            break;
        }
        
        // Only collect characters if we're inside quotes
        if (insideQuotes) {
            // any digit is allowed
            // '+' is only allowed as the first character of the phone number
            if (isdigit(c) || (c == '+' && phoneIdx == 0)) {
                phoneNumber[phoneIdx++] = c;
            } else {
                LOGE("Unexpected character in phone number: '%c'", c);
                return;
            }
        }
    }
    
    phoneNumber[phoneIdx] = '\0'; // Ensure null termination

    // return phone number is empty
    if (phoneNumber[0] == '\0') {
        LOGE("Return phone number is empty");
        return;
    }

    // parse command from SMS content
    if (str_case_cmp(cmd, "get location") == 0) {
        LOGI("Recognized 'get location' command from %s", phoneNumber);
        SendLocationSms(phoneNumber);
    } else {
        LOGE("Unknown SMS command received: %s", cmd);
    }
}

void SendLocationSms(const char* phoneNumber)
{
    char msg[SMS_BODY_MAX_LEN];
    if (GetGoogleMapsLink(msg, sizeof(msg))) {
        LOGI("Sending location SMS to %s: %s", phoneNumber, msg);
        SMS_SendMessage(phoneNumber, (const uint8_t*)msg, strlen(msg), SIM0);
    } else {
        const char* err = "Location not available";
        LOGI("Sending error SMS to %s: %s", phoneNumber, err);
        SMS_SendMessage(phoneNumber, (const uint8_t*)err, strlen(err), SIM0);
    }
}
