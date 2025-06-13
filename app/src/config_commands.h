#ifndef CONFIG_COMMANDS_H
#define CONFIG_COMMANDS_H

#include "config_store.h"

void HandleUartCommand(char* cmd);
void SmsListMessageCallback(SMS_Message_Info_t* msg);

#endif
