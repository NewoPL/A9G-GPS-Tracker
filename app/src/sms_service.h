#ifndef SMS_SERVICE_H
#define SMS_SERVICE_H

void SmsInit();
// Callback for SMS reception
// This function will be called when an SMS is received
// it parses the SMS header and content,
// extracts the phone number and command, and processes it accordingly.
void SmsReceivedCallback(SMS_Encode_Type_t encodeType,
                         const char* headerStr,
                         const char* contentStr,
                         uint32_t    contentLen);
                         
// Forward declaration for sending location SMS
void SendLocationSms(const char* phoneNumber);

#endif // SMS_SERVICE_H
