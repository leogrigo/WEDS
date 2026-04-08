#pragma once

#include <Arduino.h>
#include <cstdint>

typedef struct anomaly_result_t {
    float temp_baseline;
    float hum_baseline;
    float gas_baseline;

    float temp_stddev;
    float hum_stddev;
    float gas_stddev;

    float temp_score;
    float hum_score;
    float gas_score;

    float global_score;
} anomaly_result_t;

enum anomaly_state {
    ANOMALY_STARTUP,
    ANOMALY_WARMUP,
    NO_ANOMALY,
    ANOMALY_WARNING,
    ANOMALY_ALERT
};

void printAnomalyResults(anomaly_result_t result);
