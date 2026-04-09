#pragma once


typedef struct sensors_sample_t {
    float temp;
    float hum;
    float press;
    float gas_r;
} sensors_sample_t;


#include "anomalies.hpp"
typedef struct node_state_t {
    uint64_t node_id;
    float sample_freq; // Samples per minute
    int samples_seen;
    anomaly_state an_state;
    sensors_sample_t anomaly_last_proc_sample;
    anomaly_result_t anomaly_last_result;
} node_state_t;

extern node_state_t state;
