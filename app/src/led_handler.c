#include <api_os.h>
#include <api_hal_gpio.h>
#include "system.h"
#include "led_handler.h"

static GPIO_config_t gpioLedGps = {
    .mode         = GPIO_MODE_OUTPUT,
    .pin          = GPS_STATUS_LED,
    .defaultLevel = GPIO_LEVEL_LOW
};

static GPIO_config_t gpioLedGsm = {
    .mode         = GPIO_MODE_OUTPUT,
    .pin          = GSM_STATUS_LED,
    .defaultLevel = GPIO_LEVEL_LOW
};

void LED_Blink(void* param);

void LED_init()
{  
    GPIO_Init(gpioLedGps);
    GPIO_Init(gpioLedGsm);
}

void LED_cycle_start(HANDLE taskHandle)
{  
    OS_StartCallbackTimer(taskHandle, 500, LED_Blink, (void*)taskHandle);
}

static void handle_led_blink(bool status_on, int count, int pin)
{
    if (status_on) {
        GPIO_Set(pin, GPIO_LEVEL_HIGH);
    } else {
        if (count == 0)
            GPIO_Set(pin, GPIO_LEVEL_HIGH);
        else if (count == 1)
            GPIO_Set(pin, GPIO_LEVEL_LOW);
    }
}

void LED_Blink(void* param)
{
    static int count = 0;
    HANDLE taskHandle = (HANDLE)param;
    if (taskHandle == NULL) return;

    if (IS_INITIALIZED()) {
        handle_led_blink(IS_GPS_FIX(), count, GPS_STATUS_LED);
        handle_led_blink(IS_GSM_STATUS_ON(), count, GSM_STATUS_LED);
        count = (count + 1) % 5;
    } else {
        GPIO_Set(GPS_STATUS_LED, GPIO_LEVEL_LOW);
        handle_led_blink(false, count, GSM_STATUS_LED);
        count = (count + 1) % 2;
    }

    LED_cycle_start(taskHandle);
}
