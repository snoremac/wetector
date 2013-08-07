#ifndef UI_H
#define UI_H

#include <inttypes.h>

#include "common.h"
#include "gpio.h"

enum led_state {
  LED_OFF, LED_ON
};

enum led_colour {
  OFF, RED, GREEN, YELLOW, BLUE
};

void ui_init(void);

void ui_set_power_on(void);

void ui_set_status(uint8_t colour, bool blink);

void ui_set_alarm_on(void);

void ui_set_alarm_off(void);

#endif
