
#include <inttypes.h>
#include <stdlib.h>

#include "dht11.h"
#include "hal_gpio.h"
#include "hum_temp.h"
#include "log.h"
#include "notifier.h"

#define EVENT_TYPE_HUMIDITY 0x06
#define DHT11_POLL_INTERVAL 2000

struct hum_temp_context {
  struct gpio gpio;
  hum_temp_callback on_result;
};

bool read_on_tick(time_t scheduled_time, struct task* task);

void hum_temp_read(struct gpio* gpio, hum_temp_callback on_result) {
  struct hum_temp_context* context = malloc(sizeof(struct hum_temp_context));
  if (null(context)) {
    set_system_error();
    log_error_malloc();
    return;
  }
  
  context->gpio = *gpio;
  context->on_result = on_result;

  dht11_signal_start(gpio);
  struct task_config read_task_config = { "dhtrd", TASK_ONCE, 18 };
	notifier_add_task(&read_task_config, read_on_tick, NULL, context);
}

bool read_on_tick(time_t scheduled_time, struct task* task) {
  uint8_t dht11_data[5];
  struct hum_temp_context* context = (struct hum_temp_context*) task->data;
  result_t result  = dht11_read(&(context->gpio), dht11_data);
  if (result == RESULT_SUCCESS) {
    struct hum_temp_reading reading = { dht11_data[0], dht11_data[2] };
    context->on_result(&reading);
  }
  free(context);
  context = NULL;
  return false;
}
