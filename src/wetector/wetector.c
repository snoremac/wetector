
#include <inttypes.h>

#include "dht11.h"
#include "sensimatic.h"
#include "hal_gpio.h"
#include "hum_temp.h"
#include "log.h"
#include "notifier.h"
#include "sample_buffer.h"
#include "shell.h"
#include "util.h"

static struct gpio sensor_gpio = { .port  = GPIO_PORT_C, .pin = GPIO_PIN_0 };
static struct gpio led_gpio = { .port = GPIO_PORT_B, .pin = GPIO_PIN_5 };

uint8_t hum_temp_task_id;

static uint8_t humidity_20_sec_samples[10];
static struct sample_buffer humidity_20_sec_buffer;
static uint8_t humidity_10_min_samples[300];
static struct sample_buffer humidity_10_min_buffer;

static uint8_t temperature_20_sec_samples[10];
static struct sample_buffer temperature_20_sec_buffer;
static uint8_t temperature_10_min_samples[300];
static struct sample_buffer temperature_10_min_buffer;

static struct sample_buffer* sample_buffers[4] = { 
  &humidity_20_sec_buffer, &humidity_10_min_buffer,
  &temperature_20_sec_buffer, &temperature_10_min_buffer
};

static shell_result_t shell_handler(shell_command_t* command);

static bool humidity_on_tick(time_t time, struct task* task);
static void humidity_on_complete(struct hum_temp_reading* reading);

static void start();
static void stop();

static uint16_t eeprom_load(void);
static uint16_t eeprom_save(void);

static void print_stats(void);
static void print_samples(void);

bool setup_handler(time_t scheduled_time, struct task* task) {
  sample_buffer_init(&humidity_20_sec_buffer, humidity_20_sec_samples, ARRAY_SIZE(humidity_20_sec_samples));
  sample_buffer_init(&humidity_10_min_buffer, humidity_10_min_samples, ARRAY_SIZE(humidity_10_min_samples));

  sample_buffer_init(&temperature_20_sec_buffer, temperature_20_sec_samples, ARRAY_SIZE(temperature_20_sec_samples));
  sample_buffer_init(&temperature_10_min_buffer, temperature_10_min_samples, ARRAY_SIZE(temperature_10_min_samples));

  gpio_set_mode(&led_gpio, GPIO_OUTPUT_NORMAL);
  shell_register_handler("ht", shell_handler);
  
  start();
  return false;
}

static bool humidity_on_tick(time_t time, struct task* task) {
  gpio_write(&led_gpio, LOGIC_HIGH);
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
  
  gpio_write(&led_gpio, LOGIC_LOW);
}

static void start() {
  struct task_config humidity_task_config = { "hum", TASK_FOREVER, 2000 };
	hum_temp_task_id = notifier_add_task(&humidity_task_config, humidity_on_tick, NULL, NULL); 
}

static void stop() {
	notifier_remove_task(hum_temp_task_id);   
}

static uint16_t eeprom_load() {
  uint16_t pos = 0;
  for (uint8_t i = 0; i < ARRAY_SIZE(sample_buffers); i++) {
    sample_buffer_init(sample_buffers[i], sample_buffers[i]->samples, sample_buffers[i]->size);
    eeprom_read_buffer(sample_buffers[i], pos);
    pos += sample_buffers[i]->count + 2;
  }
  return pos;
}

static uint16_t eeprom_save() {
  uint16_t pos = 0;
  for (uint8_t i = 0; i < ARRAY_SIZE(sample_buffers); i++) {
    eeprom_write_buffer(sample_buffers[i], pos);
    pos += sample_buffers[i]->count + 2;
  }
  return pos;
}

static shell_result_t shell_handler(shell_command_t* command) {
	if (command->args_count == 0) return SHELL_RESULT_FAIL;
	
	if (string_eq(command->command, "ht")) {
    if (string_eq(command->args[0], "start")) {
      start();
		} else if (string_eq(command->args[0], "stop")) {
      stop();
		} else if (string_eq(command->args[0], "save")) {
      uint16_t bytes_written = eeprom_save();
      shell_printf("%u samples written\n", bytes_written);
		} else if (string_eq(command->args[0], "load")) {
      uint16_t bytes_read = eeprom_load();
      shell_printf("%u samples read\n", bytes_read);
		} else if (string_eq(command->args[0], "stats")) {
      print_stats();
		} else if (string_eq(command->args[0], "samples")) {
      print_samples();
		} else {
			return SHELL_RESULT_FAIL;
		}
	}
	return SHELL_RESULT_SUCCESS;
}

static void print_stats() {
  shell_printf("\nMetric\t\t20 sec av\t10 min av\n");
  
  uint32_t temp_20_sec = temperature_20_sec_buffer.sum / temperature_20_sec_buffer.count;
  uint32_t temp_10_min = temperature_10_min_buffer.sum / temperature_10_min_buffer.count;
  shell_printf("Temperature\t%lu C\t\t%lu C\n", temp_20_sec, temp_10_min);
      
  uint32_t hum_20_sec = humidity_20_sec_buffer.sum / humidity_20_sec_buffer.count;
  uint32_t hum_10_min = humidity_10_min_buffer.sum / humidity_10_min_buffer.count;
  shell_printf("Humidity\t%lu %%\t\t%lu %%\n", hum_20_sec, hum_10_min);

  shell_printf("\nMetric\t\t20 sec cnt\t20 sec sum\t10 min cnt\t10 min sum\n");

  shell_printf("Temperature\t%u\t\t%lu\t\t%u\t\t%lu\t\t\n",
    temperature_20_sec_buffer.count,
    temperature_20_sec_buffer.sum,
    temperature_10_min_buffer.count,
    temperature_10_min_buffer.sum
  );
    shell_printf("Humidity\t%u\t\t%lu\t\t%u\t\t%lu\t\t\n",
    humidity_20_sec_buffer.count,
    humidity_20_sec_buffer.sum,
    humidity_10_min_buffer.count,
    humidity_10_min_buffer.sum
  );
      
  shell_printf("\n"); 
}

static void print_samples() {
  FILE* shell_stream = shell_get_stream();
  shell_printf("Humidity 20 sec:\n");
  print_sample_buffer(&humidity_20_sec_buffer, shell_stream);
  shell_printf("Humidity 10 min:\n");
  print_sample_buffer(&humidity_10_min_buffer, shell_stream);
  shell_printf("Temperature 20 sec:\n");
  print_sample_buffer(&temperature_20_sec_buffer, shell_stream);
  shell_printf("Temperature 10 min:\n");
  print_sample_buffer(&temperature_10_min_buffer, shell_stream);
}

