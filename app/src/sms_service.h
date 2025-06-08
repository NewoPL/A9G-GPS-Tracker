#ifndef SMS_SERVICE_H
#define SMS_SERVICE_H

void SMSInit();
void HandleSmsReceived(API_Event_t* pEvent);
// Forward declaration for sending location SMS
void SendLocationSms(const char* phoneNumber);

#endif // SMS_SERVICE_H
