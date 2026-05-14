#pragma once

#define WEDS_MQ2_PIN 4
#define WEDS_I2C_SDA 41
#define WEDS_I2C_SCL 42

#include "WedsSensorSample.h"

void weds_sensors_begin();
WedsSensorSample weds_read_environment_sample();
