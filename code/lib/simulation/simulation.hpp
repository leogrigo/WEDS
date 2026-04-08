#pragma once
#include <Arduino.h>
#include "states.hpp"

enum sim_mode_t {
    SIM_MODE_NORMAL,
    SIM_MODE_DRY_PERIOD,
    SIM_MODE_GAS_DROP,
    SIM_MODE_FIRE_EVENT,
    SIM_MODE_SENSOR_FAULT
};

extern const sim_mode_t SELECTED_MODE;

typedef struct sim_state_t {
    uint32_t tick;
    uint32_t mode_tick;          // counts samples after warmup
    bool in_warmup;

    // Sensor fault support
    bool fault_initialized;
    float fault_temp_value;
    float fault_hum_value;
    float fault_press_value;
    float fault_gas_r_value;
} sim_state_t;

extern sim_state_t sim_state;

void updateSimulationState(sim_state_t* s);
sensors_sample_t generateSimulatedSample(sim_state_t* s);
