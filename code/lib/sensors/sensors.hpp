#pragma once
#include "states.hpp"
#define MQ2_PIN 4
#define I2C_SDA 41
#define I2C_SCL 42

void sensors_begin();
sensors_sample_t sens_enviroment();

