#include <inttypes.h>
#include <stdlib.h>

#include "common.h"
#include "event/gpio_event.h"
#include "hal_gpio.h"
#include "log.h"
#include "notifier.h"
#include "scheduler.h"
#include "ui.h"

static struct gpio status_red_gpio = { .port = GPIO_PORT_D, .pin = GPIO_PIN_5 };
static struct gpio status_green_gpio = { .port = GPIO_PORT_D, .pin = GPIO_PIN_6 };
static struct gpio status_blue_gpio = { .port = GPIO_PORT_B, .pin = GPIO_PIN_1 };
static struct gpio speaker_gpio = { .port = GPIO_PORT_B, .pin = GPIO_PIN_3 };
static struct gpio power_gpio = { .port = GPIO_PORT_B, .pin = GPIO_PIN_4 };
static struct gpio reset_gpio = { .port = GPIO_PORT_B, .pin = GPIO_PIN_0 };
static struct gpio test_gpio = { .port = GPIO_PORT_D, .pin = GPIO_PIN_4 };

static struct gpio* status_gpios[] = { &status_red_gpio, &status_green_gpio, &status_blue_gpio };

static uint8_t status_gpio_color_levels[5][3] = {
  { 0, 0, 0 },
  { 100, 0, 0 },
  { 0, 100, 0 },
  { 100, 22, 0 },
  { 0, 0, 100 }
};

static uint8_t status_task_id;
static uint8_t status_state;

static uint8_t alarm_task_id;
static uint8_t alarm_tone;

static bool on_reset_button(event_t* event);
static bool on_test_button(event_t* event);

static bool status_on_tick(time_t time, struct task* task);
static bool alarm_on_tick(time_t time, struct task* task);

void ui_init(void) {
  gpio_set_mode(&power_gpio, GPIO_OUTPUT);
  ui_set_power_on();

  gpio_set_mode(&status_red_gpio, GPIO_OUTPUT);
  gpio_set_mode(&status_green_gpio, GPIO_OUTPUT);
  gpio_set_mode(&status_blue_gpio, GPIO_OUTPUT);
  ui_set_status(OFF, false);
  
  gpio_set_mode(&speaker_gpio, GPIO_OUTPUT);
  alarm_tone = 0;
  
  gpio_set_mode(&reset_gpio, GPIO_INPUT);
  gpio_event_add_listener(&reset_gpio, on_reset_button);

  gpio_set_mode(&test_gpio, GPIO_INPUT);
  gpio_event_add_listener(&test_gpio, on_test_button);
}

void ui_set_power_on(void) {
  gpio_write(&power_gpio, LOGIC_HIGH);
}

void ui_set_status(uint8_t colour, bool blink) {
  scheduler_remove_task(status_task_id);
  for (uint8_t i = 0; i < 3; i++) {
    gpio_set_duty_cycle(status_gpios[i], status_gpio_color_levels[colour][i]);
  }
  if (colour == OFF) {
    status_state = LED_OFF;
  } else {
    status_state = LED_ON;
  }
  if (blink) {
    struct task_config status_task_config = { "stbl", TASK_FOREVER, 800 };
    status_task_id = notifier_add_task(&status_task_config, status_on_tick, NULL, status_gpio_color_levels[colour]); 
  }
}

static bool status_on_tick(time_t time, struct task* task) {
  uint8_t* colour_levels = (uint8_t*) task->data;
  if (status_state == LED_ON) {
    for (uint8_t i = 0; i < 3; i++) {
      gpio_set_duty_cycle(status_gpios[i], 0);
    } 
    status_state = LED_OFF;
  } else {
    for (uint8_t i = 0; i < 3; i++) {
      gpio_set_duty_cycle(status_gpios[i], colour_levels[i]);
    } 
    status_state = LED_ON;    
  }
  return true;
}

static bool on_reset_button(event_t* event) {
	gpio_event_t* gpio_event = (gpio_event_t*) event;
	if (gpio_event->super.descriptor == gpio_to_descriptor(&reset_gpio)) {
		if (gpio_event->event_type == GPIO_UP) {
      system_reset();
    }
  }
  return true;
}

static bool on_test_button(event_t* event) {
	gpio_event_t* gpio_event = (gpio_event_t*) event;
	if (gpio_event->super.descriptor == gpio_to_descriptor(&test_gpio)) {
		if (gpio_event->event_type == GPIO_DOWN) {
      ui_set_alarm_on();
    } else {
      ui_set_alarm_off();      
    }
  }
  return true;
}

void ui_set_alarm_on(void) {
  alarm_task_id = notifier_add_task(&(struct task_config){"alarm", TASK_FOREVER, 250}, alarm_on_tick, NULL, NULL);
}

void ui_set_alarm_off(void) {
  notifier_remove_task(alarm_task_id);
  gpio_set_frequency(&speaker_gpio, 0);
}

static bool alarm_on_tick(time_t time, struct task* task) {
	gpio_set_frequency(&speaker_gpio, alarm_tone);
  if (alarm_tone == 0) {
    alarm_tone = 100;
  } else {
    alarm_tone -= 20;
  }
  return true;
}
