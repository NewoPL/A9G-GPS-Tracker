#ifndef LED_HANDLER_H
#define LED_HANDLER_H

#define GPS_STATUS_LED   GPIO_PIN27
#define GSM_STATUS_LED   GPIO_PIN28

void LED_init();
void LED_cycle_start(HANDLE taskHandle);

#endif