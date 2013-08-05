#ifndef HUM_TEMP_H
#define HUM_TEMP_H

#include <inttypes.h>
#include <stdio.h>

#include "common.h"
#include "clock.h"
#include "notifier.h"

struct hum_temp_reading {
  uint8_t humidity;
  uint8_t temperature;
};

struct hum_temp_stats {
  uint8_t humidity_av_20_sec;
  uint8_t humidity_av_10_min;
  uint8_t temperature_av_20_sec;
  uint8_t temperature_av_10_min;
};

struct hum_temp_read_event {
	event_t super;
  struct hum_temp_reading reading; 
};

struct hum_temp_change_event {
	event_t super;
  struct hum_temp_stats stats; 
};

void hum_temp_init(void);

void hum_temp_read(event_handler on_reading);

void hum_temp_calibrate(event_handler on_complete);

void hum_temp_start_collector(void);

void hum_temp_stop_collector(void);

void hum_temp_start_monitor(event_handler on_change);

void hum_temp_stop_monitor(void);

uint16_t hum_temp_load(void);

uint16_t hum_temp_save(void);

struct hum_temp_stats hum_temp_current_stats(void);

void hum_temp_print_stats(FILE* stream);

void hum_temp_print_samples(FILE* stream);

#endif
