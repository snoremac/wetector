
#include <inttypes.h>
#include <stdlib.h>

#include "common.h"
#include "dht11.h"
#include "hum_temp.h"
#include "log.h"
#include "wetector/pgm_strings.h"
#include "hal/hal.h"
#include "sample_buffer.h"

#define EVENT_TYPE_HUM_TEMP 0x06
#define EVENT_DESCRIPTOR_HUM_TEMP_READING 0x00
#define EVENT_DESCRIPTOR_HUM_TEMP_CHANGE 0x01
#define EVENT_DESCRIPTOR_HUM_TEMP_CALIBRATE 0x02

#define CALIBRATE_SAMPLES 10

#define DHT11_POLL_INTERVAL 2000

static struct gpio sensor_gpio = { .port  = GPIO_PORT_C, .pin = GPIO_PIN_0 };

uint8_t monitor_task_id;
uint8_t collector_task_id;

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

static struct hum_temp_read_event current_read_event = { 
  .super.type = EVENT_TYPE_HUM_TEMP,
  .super.descriptor = EVENT_DESCRIPTOR_HUM_TEMP_READING
};
static struct hum_temp_change_event current_change_event = {
  .super.type = EVENT_TYPE_HUM_TEMP,
  .super.descriptor = EVENT_DESCRIPTOR_HUM_TEMP_CHANGE
};
static struct event current_calibrate_event = {
  .type = EVENT_TYPE_HUM_TEMP,
  .descriptor = EVENT_DESCRIPTOR_HUM_TEMP_CALIBRATE
};

static void calibrate_task(struct task* task);
static void calibrate_complete_task(struct task* task);
static void collector_task(struct task* task);
static void monitor_task(struct task* task);
static void hum_temp_complete_read(struct task* task);
static bool on_hum_temp_reading(event_t* event);

void hum_temp_init() {
  sample_buffer_init(&humidity_20_sec_buffer, humidity_20_sec_samples, ARRAY_SIZE(humidity_20_sec_samples));
  sample_buffer_init(&humidity_10_min_buffer, humidity_10_min_samples, ARRAY_SIZE(humidity_10_min_samples));

  sample_buffer_init(&temperature_20_sec_buffer, temperature_20_sec_samples, ARRAY_SIZE(temperature_20_sec_samples));
  sample_buffer_init(&temperature_10_min_buffer, temperature_10_min_samples, ARRAY_SIZE(temperature_10_min_samples));
}

void hum_temp_calibrate(event_handler on_complete) {
  event_add_listener(EVENT_TYPE_HUM_TEMP, EVENT_DESCRIPTOR_HUM_TEMP_CALIBRATE, on_complete);
  struct task_config calibrate_task_config = { "htcal", CALIBRATE_SAMPLES, DHT11_POLL_INTERVAL };
	scheduler_add_task(&calibrate_task_config, calibrate_task, NULL); 
}

static void calibrate_task(struct task* task) {
  hum_temp_read(on_hum_temp_reading);
  if (task->times_run == CALIBRATE_SAMPLES) {
    struct task_config calibrate_task_config = { "htcal", TASK_ONCE, TASK_ASAP };
    scheduler_add_task(&calibrate_task_config, calibrate_complete_task, NULL);     
  }
}

static void calibrate_complete_task(struct task* task) {
  struct hum_temp_stats current_stats = hum_temp_current_stats();

  for (uint16_t i = 0; i < humidity_20_sec_buffer.size; i++) {
    push_sample(&humidity_20_sec_buffer, current_stats.humidity_av_20_sec);
  }
  for (uint16_t i = 0; i < humidity_10_min_buffer.size; i++) {
    push_sample(&humidity_10_min_buffer, current_stats.humidity_av_20_sec);
  }
  for (uint16_t i = 0; i < temperature_20_sec_buffer.size; i++) {
    push_sample(&temperature_20_sec_buffer, current_stats.temperature_av_20_sec);
  }
  for (uint16_t i = 0; i < temperature_10_min_buffer.size; i++) {
    push_sample(&temperature_10_min_buffer, current_stats.temperature_av_20_sec);
  }
  
  event_fire_event(&current_calibrate_event);
}

void hum_temp_start_collector() {
  struct task_config collector_task_config = { "htcol", TASK_FOREVER, DHT11_POLL_INTERVAL };
	collector_task_id = scheduler_add_task(&collector_task_config, collector_task, NULL);   
}

void hum_temp_stop_collector() {
	scheduler_remove_task(collector_task_id);
}

void hum_temp_start_monitor(event_handler on_change) {
  struct task_config monitor_task_config = { "htmon", TASK_FOREVER, DHT11_POLL_INTERVAL * 10 };
	monitor_task_id = scheduler_add_task(&monitor_task_config, monitor_task, NULL);
  event_add_listener(EVENT_TYPE_HUM_TEMP, EVENT_DESCRIPTOR_HUM_TEMP_CHANGE, on_change);
}

void hum_temp_stop_monitor() {
	scheduler_remove_task(monitor_task_id);   
}

static void monitor_task(struct task* task) {
  struct hum_temp_stats stats = hum_temp_current_stats();
  int8_t humidity_change = stats.humidity_av_20_sec - stats.humidity_av_10_min;
  LOG_INFO("Humidity difference: %i\n", humidity_change);
  if (humidity_change > 10) {
    LOG_INFO("Humidity difference beyond threshold: %i\n", humidity_change);
    current_change_event.stats = stats;
    event_fire_event((event_t*) &current_change_event);
  }
}

static void collector_task(struct task* task) {
  hum_temp_read(on_hum_temp_reading);
}

void hum_temp_read(event_handler on_reading) {
  event_add_listener(EVENT_TYPE_HUM_TEMP, EVENT_DESCRIPTOR_HUM_TEMP_READING, on_reading);
  dht11_signal_start(&sensor_gpio);
  struct task_config read_task_config = { "dhtrd", TASK_ONCE, 18 };
	scheduler_add_task(&read_task_config, hum_temp_complete_read, &sensor_gpio);
}

static void hum_temp_complete_read(struct task* task) {
  uint8_t dht11_data[5];
  result_t result = dht11_read((struct gpio*) task->data, dht11_data);
  
  if (result == RESULT_SUCCESS) {
    struct hum_temp_reading reading = { dht11_data[0], dht11_data[2] };
    current_read_event.reading = reading;
  } else {
    current_read_event.reading = (struct hum_temp_reading) { 0 };    
  }
  event_fire_event((event_t*) &current_read_event);
}

static bool on_hum_temp_reading(event_t* event) {
  struct hum_temp_read_event* read_event = (struct hum_temp_read_event*) event;
  struct hum_temp_reading current_reading = read_event->reading;
  if (current_reading.temperature == 0 && current_reading.humidity == 0) {
    LOG_ERROR("No humidity / temperature reading received\n", "");
  }

  push_sample(&humidity_20_sec_buffer, current_reading.humidity);
  push_sample(&humidity_10_min_buffer, current_reading.humidity);

  push_sample(&temperature_20_sec_buffer, current_reading.temperature);
  push_sample(&temperature_10_min_buffer, current_reading.temperature);

  return false;
}

uint16_t hum_temp_load(void) {
  uint16_t pos = 0;
  for (uint8_t i = 0; i < ARRAY_SIZE(sample_buffers); i++) {
    sample_buffer_init(sample_buffers[i], sample_buffers[i]->samples, sample_buffers[i]->size);
    eeprom_read_buffer(sample_buffers[i], pos);
    pos += sample_buffers[i]->count + 2;
  }
  return pos;
}

uint16_t hum_temp_save(void) {
  uint16_t pos = 0;
  for (uint8_t i = 0; i < ARRAY_SIZE(sample_buffers); i++) {
    eeprom_write_buffer(sample_buffers[i], pos);
    pos += sample_buffers[i]->count + 2;
  }
  return pos;
}

struct hum_temp_stats hum_temp_current_stats(void) {
  struct hum_temp_stats stats = { 0 };
  if (humidity_20_sec_buffer.count > 0) {
    stats.humidity_av_20_sec = humidity_20_sec_buffer.sum / humidity_20_sec_buffer.count;    
  }
  if (humidity_10_min_buffer.count > 0) {
    stats.humidity_av_10_min = humidity_10_min_buffer.sum / humidity_10_min_buffer.count;
  }
  if (temperature_20_sec_buffer.count > 0) {
    stats.temperature_av_20_sec = temperature_20_sec_buffer.sum / temperature_20_sec_buffer.count;    
  }
  if (temperature_10_min_buffer.count > 0) {
    stats.temperature_av_10_min = temperature_10_min_buffer.sum / temperature_10_min_buffer.count;
  }
  return stats;
}

void hum_temp_print_stats(FILE* stream) {
  struct hum_temp_stats stats = hum_temp_current_stats();
  
  WT_PGM_STR(WETECTOR_SHELL_STATS_HEADER, shell_stats_header);
  WT_PGM_STR(WETECTOR_SHELL_STATS_ROW, shell_stats_row);
  
  fprintf(stream, shell_stats_header);
  fprintf(stream, shell_stats_row, "H", stats.humidity_av_20_sec, stats.humidity_av_10_min);
  fprintf(stream, shell_stats_row, "T", stats.temperature_av_20_sec, stats.temperature_av_10_min);

  fputc('\n', stream);
}

void hum_temp_print_samples(FILE* stream) {
  WT_PGM_STR(WETECTOR_SHELL_SAMPLES_20_SEC, shell_samples_20_sec);
  WT_PGM_STR(WETECTOR_SHELL_SAMPLES_10_MIN, shell_samples_10_min);
  
  fprintf(stream, shell_samples_20_sec, "H");
  print_sample_buffer(&humidity_20_sec_buffer, stream);
  fprintf(stream, shell_samples_10_min, "H");
  print_sample_buffer(&humidity_10_min_buffer, stream);
  fprintf(stream, shell_samples_20_sec, "T");
  print_sample_buffer(&temperature_20_sec_buffer, stream);
  fprintf(stream, shell_samples_10_min, "T");
  print_sample_buffer(&temperature_10_min_buffer, stream);
}


