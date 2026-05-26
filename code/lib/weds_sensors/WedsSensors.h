#pragma once

#include "WedsSensorSample.h"
#include <stdint.h>
#define WEDS_I2C_SDA 41
#define WEDS_I2C_SCL 42

#ifdef WEDS_LEGACY_SENSORS

#define WEDS_MQ2_PIN 4

/**
 * @brief Initializes the legacy environmental sensors.
 */
void weds_sensors_begin();

/**
 * @brief Reads a new sample from the legacy sensors.
 * @return WedsSensorSample The collected sensor reading.
 */
WedsSensorSample weds_read_environment_sample();

#else

/**
 * @brief Initializes the BME68X environmental sensor.
 * @return true if successful, false otherwise.
 */
bool weds_sensors_begin();

/**
 * @brief Saves the current BSEC state.
 * @return true if successful, false otherwise.
 */
bool weds_sensor_save_state();

/**
 * @brief Loads the BSEC state from the provided buffer.
 * @param state Pointer to the state buffer.
 * @return true if successful, false otherwise.
 */
bool weds_sensor_load_state(uint8_t *state);

/**
 * @brief Reads a new sample from the BME68X sensor.
 * @return WedsSensorSample The collected sensor reading.
 */
WedsSensorSample weds_read_environment_sample();

/**
 * @brief Retrieves the time until the next sensor reading is due.
 * @return long long Next call time in milliseconds.
 */
int64_t weds_sensor_next_call_ms();

#endif

/**
 * @brief Prints the sensor sample to the serial console.
 * @param sample The sensor sample to print.
 */
void weds_print_sample(const WedsSensorSample& sample);