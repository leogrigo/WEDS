#pragma once

#include "anomalies.hpp"

typedef struct sensors_sample_t {
    float temp;
    float hum;
    float press;
    float gas_r;
} sensors_sample_t;

typedef struct risk_result_t {
    float global_score;
} risk_result_t;

typedef struct node_state_t {
    float sample_freq; // Samples per minute
    int samples_seen;
    int anomaly_warning_streak;
    anomaly_state an_state;
    sensors_sample_t anomaly_last_proc_sample;
    anomaly_result_t anomaly_last_result;
} node_state_t;
