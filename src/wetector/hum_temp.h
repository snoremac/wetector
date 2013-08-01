#ifndef HUM_TEMP_H
#define HUM_TEMP_H

#include <inttypes.h>

#include "common.h"
#include "clock.h"

struct hum_temp_reading {
  uint8_t humidity;
  uint8_t temperature;
};

struct hum_temp_stats {
  uint16_t humidity_sum;
  uint16_t humidity_count;
  uint16_t temperature_sum;
  uint16_t temperature_count;
};

typedef void (*hum_temp_callback)(struct hum_temp_reading* reading);

void hum_temp_read(struct gpio* gpio, hum_temp_callback on_result);

#endif
