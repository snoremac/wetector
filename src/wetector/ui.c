#include <inttypes.h>
#include <stdlib.h>

#include "common.h"
#include "event.h"
#include "gpio.h"
#include "notifier.h"
#include "scheduler.h"
#include "ui.h"

static struct gpio status_red_gpio = { .port = GPIO_PORT_D, .pin = GPIO_PIN_5 };
static struct gpio status_green_gpio = { .port = GPIO_PORT_D, .pin = GPIO_PIN_6 };
static struct gpio status_blue_gpio = { .port = GPIO_PORT_D, .pin = GPIO_PIN_7 };
static struct gpio speaker_gpio = { .port = GPIO_PORT_B, .pin = GPIO_PIN_3 };
static struct gpio power_gpio = { .port = GPIO_PORT_B, .pin = GPIO_PIN_4 };
static struct gpio reset_gpio = { .port = GPIO_PORT_D, .pin = GPIO_PIN_2 };

static struct gpio* status_gpios[] = { &status_red_gpio, &status_green_gpio, &status_blue_gpio };

static logic_level_t status_gpio_color_states[5][3] = {
  { LOGIC_LOW, LOGIC_LOW, LOGIC_LOW },
  { LOGIC_HIGH, LOGIC_LOW, LOGIC_LOW },
  { LOGIC_LOW, LOGIC_HIGH, LOGIC_LOW },
  { LOGIC_HIGH, LOGIC_HIGH, LOGIC_LOW },
  { LOGIC_LOW, LOGIC_LOW, LOGIC_HIGH }
};

static uint8_t status_task_ids[] = { 0, 0, 0 };
static uint8_t alarm_task_id;
static uint8_t alarm_tone;

static bool on_reset_button(event_t* event);

static bool status_on_tick(time_t time, struct task* task);
static bool alarm_on_tick(time_t time, struct task* task);

void ui_init(void) {
  gpio_set_mode(&status_red_gpio, GPIO_OUTPUT_NORMAL);
  gpio_set_mode(&status_green_gpio, GPIO_OUTPUT_NORMAL);
  gpio_set_mode(&status_blue_gpio, GPIO_OUTPUT_NORMAL);

  gpio_set_mode(&speaker_gpio, GPIO_OUTPUT_NORMAL);
  
  gpio_set_mode(&reset_gpio, GPIO_INPUT);
  event_add_listener(EVENT_TYPE_GPIO, gpio_to_descriptor(&reset_gpio), on_reset_button);

  gpio_set_mode(&power_gpio, GPIO_OUTPUT_NORMAL);

  ui_set_power_on();
}

void ui_set_power_on(void) {
  gpio_write(&power_gpio, LOGIC_HIGH);
}

void ui_set_status(uint8_t colour, bool blink) {
  for (uint8_t i = 0; i < 3; i++) {
    if (status_task_ids[i] > 0) {
      scheduler_remove_task(status_task_ids[i]);
    }
  }
  for (uint8_t i = 0; i < 3; i++) {
    gpio_write(status_gpios[i], status_gpio_color_states[colour][i]);
  }
  if (blink) {
    for (uint8_t i = 0; i < 3; i++) {
      if (status_gpio_color_states[colour][i] == LOGIC_HIGH) {
        struct task_config status_task_config = { "stbl", TASK_FOREVER, 800 };
        status_task_ids[i] = notifier_add_task(&status_task_config, status_on_tick, NULL, status_gpios[i]); 
      }
    }    
  }
}

static bool status_on_tick(time_t time, struct task* task) {
  struct gpio* status_gpio = (struct gpio*) task->data;
  gpio_flip(status_gpio);
  return true;
}

static bool on_reset_button(event_t* event) {
	gpio_event_t* gpio_event = (gpio_event_t*) event;
	if (gpio_event->super.descriptor == gpio_to_descriptor(&reset_gpio)) {
		if (gpio_event->event_type == GPIO_UP) {
      system_reset();
    }
  }
  return false;
}

void ui_set_alarm_on(void) {
	gpio_set_mode(&speaker_gpio, GPIO_OUTPUT_CTC);
  alarm_task_id = notifier_add_task(&(struct task_config){"alarm", TASK_FOREVER, 250}, alarm_on_tick, NULL, NULL);
}

void ui_set_alarm_off(void) {
  gpio_set_mode(&speaker_gpio, GPIO_OUTPUT_NORMAL);  
  gpio_write(&speaker_gpio, LOGIC_LOW);
}

static bool alarm_on_tick(time_t time, struct task* task) {
	gpio_set_pwm_duty_cycle(&speaker_gpio, alarm_tone);
  if (alarm_tone == 0) {
    alarm_tone = 100;
  } else {
    alarm_tone -= 20;
  }
  return true;
}
