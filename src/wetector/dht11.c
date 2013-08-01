
#include <stdio.h>
#include <inttypes.h>
#include <avr/io.h>
#include <util/delay.h>

#include "log.h"
#include "hal_gpio.h"

enum DHT11_RESULT {
  DHT11_RESULT_SUCCESS, DHT11_RESULT_FAIL_START_1, DHT11_RESULT_FAIL_START_2,
  DHT11_RESULT_FAIL_CHECKSUM
};

static void reset(const struct gpio* gpio, struct gpio_regs* regs) __attribute__((always_inline));
static result_t send_start(const struct gpio* gpio, struct gpio_regs* regs);
static uint8_t read_byte(const struct gpio* gpio, struct gpio_regs* regs);
static result_t checksum(uint8_t* data);

void dht11_signal_start(const struct gpio* gpio) {
  struct gpio_regs regs = { 0 };
  hal_gpio_port_regs(gpio, &regs);

  reset(gpio, &regs);
  *(regs.port_data_reg) &= ~_BV(gpio->pin);
}

result_t dht11_read(const struct gpio* gpio, uint8_t* data) {
  result_t result;  
  struct gpio_regs regs = { 0 };
  hal_gpio_port_regs(gpio, &regs);
  
  result = send_start(gpio, &regs);
  if (result != DHT11_RESULT_SUCCESS) return result;
  
  for (uint8_t i = 0; i < 5; i++) {
    data[i] = read_byte(gpio, &regs);
  }

  reset(gpio, &regs);

  result = checksum(data);
  if (result != DHT11_RESULT_SUCCESS) return result;

  return DHT11_RESULT_SUCCESS;
}

static void reset(const struct gpio* gpio, struct gpio_regs* regs) {
  *(regs->port_direction_reg) |= _BV(gpio->pin);
  *(regs->port_data_reg) |= _BV(gpio->pin);
}

static result_t send_start(const struct gpio* gpio, struct gpio_regs* regs) {
  *(regs->port_data_reg) |= _BV(gpio->pin);
  _delay_us(40);
  *(regs->port_direction_reg) &= ~_BV(gpio->pin);
  _delay_us(40);
  
  uint8_t current_state;
  current_state = *(regs->port_input_reg) & _BV(gpio->pin);
  if (current_state) {
    return DHT11_RESULT_FAIL_START_1;
  }

  _delay_us(80);
  current_state = *(regs->port_input_reg) & _BV(gpio->pin);
  if (!current_state) {
    return DHT11_RESULT_FAIL_START_2;
  }
  
  _delay_us(80);

  return DHT11_RESULT_SUCCESS;
}

static uint8_t read_byte(const struct gpio* gpio, struct gpio_regs* regs) {
  uint8_t result = 0;
  uint8_t wait_counter;
  for(uint8_t i = 0; i < 8; i++) {
    wait_counter = 0;
    while (!(*(regs->port_input_reg) & _BV(gpio->pin)) && ++wait_counter < 200);
    _delay_us(30);
    if (*(regs->port_input_reg) & _BV(gpio->pin)) {
      result |= (1 << (7 - i));
    }
    wait_counter = 0;
    while ((*(regs->port_input_reg) & _BV(gpio->pin)) && ++wait_counter < 200);
  }
  return result;
}

static result_t checksum(uint8_t* data) {
  uint8_t checksum = data[0] + data[1] + data[2] + data[3];
  if(data[4] != checksum) {
    return DHT11_RESULT_FAIL_CHECKSUM;
  }

  return DHT11_RESULT_SUCCESS;
}
