
#include <inttypes.h>
#include <avr/interrupt.h>

#include "dht11.h"
#include "sensimatic.h"
#include "hal_gpio.h"
#include "hum_temp.h"
#include "log.h"
#include "notifier.h"
#include "shell.h"
#include "util.h"

struct sample_buffer {
  uint16_t size;
  uint16_t start;
  uint16_t count;
  uint32_t sum;
  uint8_t* samples;
};

static struct gpio sensor_gpio = { .port  = GPIO_PORT_C, .pin = GPIO_PIN_0 };
static struct gpio led_gpio = { .port = GPIO_PORT_B, .pin = GPIO_PIN_5 };

static uint8_t humidity_20_sec_samples[10];
static struct sample_buffer humidity_20_sec_buffer;
static uint8_t humidity_10_min_samples[300];
static struct sample_buffer humidity_10_min_buffer;

static uint8_t temperature_20_sec_samples[10];
static struct sample_buffer temperature_20_sec_buffer;
static uint8_t temperature_10_min_samples[300];
static struct sample_buffer temperature_10_min_buffer;

static shell_result_t shell_handler(shell_command_t* command);

static bool humidity_on_tick(time_t time, struct task* task);
static bool flip_led_on_tick(time_t time, struct task* task);
static void humidity_on_complete(struct hum_temp_reading* reading);

static void sample_buffer_init(struct sample_buffer* buffer, uint8_t* samples, const uint16_t size);
static bool sample_buffer_full(struct sample_buffer* buffer);
static bool sample_buffer_empty(struct sample_buffer* buffer);
static bool sample_buffer_complete(struct sample_buffer* buffer);
static void push_sample(struct sample_buffer* buffer, const uint8_t sample);

bool setup_handler(time_t scheduled_time, struct task* task) {
  sample_buffer_init(&humidity_20_sec_buffer, humidity_20_sec_samples, ARRAY_SIZE(humidity_20_sec_samples));
  sample_buffer_init(&humidity_10_min_buffer, humidity_10_min_samples, ARRAY_SIZE(humidity_10_min_samples));

  sample_buffer_init(&temperature_20_sec_buffer, temperature_20_sec_samples, ARRAY_SIZE(temperature_20_sec_samples));
  sample_buffer_init(&temperature_10_min_buffer, temperature_10_min_samples, ARRAY_SIZE(temperature_10_min_samples));

  struct task_config humidity_task_config = { "hum", TASK_FOREVER, 2000 };
	notifier_add_task(&humidity_task_config, humidity_on_tick, NULL, NULL);  

  gpio_set_mode(&sensor_gpio, GPIO_OUTPUT_NORMAL);
	notifier_add_task(&(struct task_config){ "led", TASK_FOREVER, 500 }, flip_led_on_tick, NULL, NULL);

  shell_register_handler("ht", shell_handler);

  return false;
}

static bool humidity_on_tick(time_t time, struct task* task) {
  hum_temp_read(&sensor_gpio, humidity_on_complete);
  return true;
}

static void humidity_on_complete(struct hum_temp_reading* reading) {
  struct hum_temp_reading current_reading = { 0 };
  if (not_null(reading)) {
    current_reading = *reading;
  } else {
    LOG_ERROR("No humidity / temperature reading received\n", "");
  }

  push_sample(&humidity_20_sec_buffer, current_reading.humidity);
  push_sample(&humidity_10_min_buffer, current_reading.humidity);

  push_sample(&temperature_20_sec_buffer, current_reading.temperature);
  push_sample(&temperature_10_min_buffer, current_reading.temperature);
}

static bool flip_led_on_tick(time_t time, struct task* task) {
	gpio_flip(&led_gpio);
	return true;
}

static void sample_buffer_init(struct sample_buffer* buffer, uint8_t* samples, const uint16_t size) {
    buffer->size = size;
    buffer->start = 0;
    buffer->count = 0;
    buffer->samples = samples;
}
 
static bool sample_buffer_full(struct sample_buffer* buffer) {
  return buffer->count == buffer->size;
}
 
static bool sample_buffer_empty(struct sample_buffer* buffer) {
  return buffer->count == 0;
}
 
static bool sample_buffer_complete(struct sample_buffer* buffer) {
  return sample_buffer_full(buffer) && (buffer->start == 0);
}

static void push_sample(struct sample_buffer* buffer, const uint8_t sample) {
  uint8_t end = (buffer->start + buffer->count) % buffer->size;
  buffer->samples[end] = sample;
  buffer->sum += sample;
  if (buffer->count == buffer->size) {
    buffer->sum -= buffer->samples[buffer->start];
    buffer->start = (buffer->start + 1) % buffer->size;      
  } else {
    ++buffer->count;      
  }
}

static shell_result_t shell_handler(shell_command_t* command) {
  cli();
	if (command->args_count == 0) return SHELL_RESULT_FAIL;
	
	if (string_eq(command->command, "ht")) {
		if (string_eq(command->args[0], "av")) {
      shell_printf("Metric\t\t20 sec av\t10 min av\n");
      
      uint32_t temp_20_sec = temperature_20_sec_buffer.sum / temperature_20_sec_buffer.count;
      uint32_t temp_10_min = temperature_10_min_buffer.sum / temperature_10_min_buffer.count;
      shell_printf("Temperature\t%llu C\t\t%lu C\n", temp_20_sec, temp_10_min);
          
      uint32_t hum_20_sec = humidity_20_sec_buffer.sum / humidity_20_sec_buffer.count;
      uint32_t hum_10_min = humidity_10_min_buffer.sum / humidity_10_min_buffer.count;
      shell_printf("Humidity\t%lu %%\t\t%lu %%\n", hum_20_sec, hum_10_min);

		} else if (string_eq(command->args[0], "tot")) {
      shell_printf("Metric\t\t20 sec cnt\t20 sec sum\t10 min cnt\t10 min sum\n");

      //shell_printf("Sum: %u\n", temperature_10_min_buffer.sum);
      shell_printf("Temperature\t%u\t\t%lu\t\t%u\t\t%lu\t\t\n",
        temperature_20_sec_buffer.count,
        temperature_20_sec_buffer.sum,
        temperature_10_min_buffer.count,
        temperature_10_min_buffer.sum
      );
      //shell_printf("Sum: %u\n", humidity_10_min_buffer.sum);
      shell_printf("Humidity\t%u\t\t%lu\t\t%u\t\t%lu\t\t\n",
        humidity_20_sec_buffer.count,
        humidity_20_sec_buffer.sum,
        humidity_10_min_buffer.count,
        humidity_10_min_buffer.sum
      );

      /*
      shell_printf("Temperature\t%u\t\t%lu\n",
        temperature_10_min_buffer.count,
        temperature_10_min_buffer.sum
      );
      shell_printf("Humidity\t%u\t\t%lu\n",
        humidity_10_min_buffer.count,
        humidity_10_min_buffer.sum
      );
      */
		} else {
			return SHELL_RESULT_FAIL;
		}
	}
  sei();
	return SHELL_RESULT_SUCCESS;
}
