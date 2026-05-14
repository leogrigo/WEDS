#pragma once

#include <Arduino.h>

#include "WedsSensorSample.h"
#include "WedsNodeConfig.h"

struct WedsSimulationState {
    uint32_t tick;
    uint32_t mode_tick;
    bool in_warmup;
    bool fault_initialized;
    float fault_temperature;
    float fault_humidity;
    float fault_pressure;
    float fault_gas_resistance;
};

extern WedsSimulationState weds_simulation_state;

WedsSensorSample weds_read_simulated_sample();
