
#include <inttypes.h>

#include "common.h"
#include "hum_temp.h"
#include "log.h"
#include "notifier.h"
#include "shell.h"
#include "ui.h"

static void calibrate(void);
static void start(void);
static void stop(void);

static bool on_calibrate(event_t* event);
static bool on_hum_temp_change(event_t* event);

static shell_result_t shell_handler(shell_command_t* command);

bool setup_handler(time_t scheduled_time, struct task* task) {
  shell_register_handler("ht", shell_handler);
  hum_temp_init();
  ui_init();  
  calibrate();  
  return false;
}

static void calibrate() {
  hum_temp_calibrate(on_calibrate);  
  ui_set_status(YELLOW, true);
}

static bool on_calibrate(event_t* event) {
  start();
  return false;
}

static void start() {
  hum_temp_start_collector();
  hum_temp_start_monitor(on_hum_temp_change);
  ui_set_status(GREEN, false);
}

static void stop() {
  hum_temp_stop_monitor();
  hum_temp_stop_collector();
  ui_set_status(OFF, false);
}

static bool on_hum_temp_change(event_t* event) {
  ui_set_status(RED, false);
  ui_set_alarm_on();
  return false;
}

static shell_result_t shell_handler(shell_command_t* command) {
	if (command->args_count == 0) return SHELL_RESULT_FAIL;
	
	if (string_eq(command->command, "ht")) {
    if (string_eq(command->args[0], "start")) {
      start();
		} else if (string_eq(command->args[0], "stop")) {
      stop();
		} else if (string_eq(command->args[0], "save")) {
      uint16_t bytes_written = hum_temp_save();
      shell_printf("%u samples written\n", bytes_written);
		} else if (string_eq(command->args[0], "load")) {
      uint16_t bytes_read = hum_temp_load();
      shell_printf("%u samples read\n", bytes_read);
		} else if (string_eq(command->args[0], "stats")) {
      hum_temp_print_stats(shell_get_stream());
		} else if (string_eq(command->args[0], "samples")) {
      hum_temp_print_samples(shell_get_stream());
		} else if (string_eq(command->args[0], "alarm")) {
      ui_set_alarm_on();
		} else {
			return SHELL_RESULT_FAIL;
		}
	}
	return SHELL_RESULT_SUCCESS;
}

