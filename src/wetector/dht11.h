#ifndef DHT11_H
#define DHT11_H

#include <inttypes.h>

#include "common.h"
#include "hal_gpio.h"

result_t dht11_read(const struct gpio* gpio, uint8_t* data);

void dht11_signal_start(const struct gpio* gpio);

#endif
