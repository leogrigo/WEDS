#pragma once

#include <Arduino.h>

#include "WedsSensorSample.h"
#include "WedsNodeConfig.h"

/**
 * @struct WedsSimulationState
 * @brief Maintains the internal state for the sensor simulation.
 */
struct WedsSimulationState {
    uint32_t tick;                      /**< Total number of simulation ticks. */
    uint32_t mode_tick;                 /**< Number of ticks since leaving warmup phase. */
    bool in_warmup;                     /**< True if the simulation is in the warmup phase. */
    bool fault_initialized;             /**< True if a sensor fault has been initialized. */
    float fault_temperature;            /**< Temperature value during a fault. */
    float fault_humidity;               /**< Humidity value during a fault. */
    float fault_pressure;               /**< Pressure value during a fault. */
    float fault_gas_resistance;         /**< Gas resistance value during a fault. */
};

/**
 * @brief Global simulation state instance.
 */
extern WedsSimulationState weds_simulation_state;

/**
 * @brief Reads a simulated sensor sample based on the current simulation state and mode.
 * @return WedsSensorSample The generated sensor sample.
 */
WedsSensorSample weds_read_simulated_sample();
