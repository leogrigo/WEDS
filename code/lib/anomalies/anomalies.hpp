#pragma once

#include <Arduino.h>
#include <cstdint>

typedef struct anomaly_result_t {
    float gas_baseline;
    float gas_stddev;
    float gas_score;
} anomaly_result_t;

enum anomaly_state {
    ANOMALY_STARTUP,
    ANOMALY_WARMUP,
    NO_ANOMALY,
    ANOMALY_WARNING,
    // ANOMALY_ALERT
};

constexpr float GAS_WARNING_THRESHOLD = 1.5f;

void printAnomalyResults(anomaly_result_t result);

#include "states.hpp"
anomaly_result_t calculateAnomalyScore(sensors_sample_t sample);
