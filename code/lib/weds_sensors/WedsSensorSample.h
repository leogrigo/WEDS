#pragma once

#include <cstdint>

/**
 * @struct WedsSensorSample
 * @brief Represents a single reading from the environmental sensors.
 */
struct WedsSensorSample {
    uint32_t timestamp;         /**< Time the sample was taken. */
    float temperature;          /**< Temperature in Celsius. */
    float humidity;             /**< Relative humidity percentage. */
    float pressure;             /**< Atmospheric pressure in Pascals. */
    float gas_resistance;       /**< Gas resistance reading. */
    bool valid = false;         /**< True if the reading is considered valid. */
};