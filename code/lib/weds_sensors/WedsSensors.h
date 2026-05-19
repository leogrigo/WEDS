#pragma once

#include "WedsSensorSample.h"
#define WEDS_I2C_SDA 41
#define WEDS_I2C_SCL 42

#ifdef WEDS_LEGACY_SENSORS

#define WEDS_MQ2_PIN 4

void weds_sensors_begin();
WedsSensorSample weds_read_environment_sample();

#else

bool weds_sensors_begin();
bool weds_sensor_save_state();
bool weds_sensor_load_state(uint8_t *state);
WedsSensorSample weds_read_environment_sample();
int64_t weds_sensor_next_call_ms();

#endif