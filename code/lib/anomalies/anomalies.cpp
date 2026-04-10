#include "anomalies.hpp"
// #include "states.hpp"
#include "utils.hpp"

EMA<4> gas_filter;
EMA<4> gas_variance_filter;

anomaly_result_t calculateAnomalyScore(sensors_sample_t sample){
    anomaly_result_t result, prev;

    state.samples_seen++;

    // First sample: initialize filters, no anomaly score yet
    if (state.an_state == ANOMALY_STARTUP) {
        result.gas_baseline  = gas_filter(sample.gas_r);
        result.gas_stddev  = 0.0f;
        result.gas_score  = 0.0f;

        state.anomaly_last_proc_sample = sample;
        state.anomaly_last_result = result;
        state.an_state = ANOMALY_WARMUP;

        Serial.println("Warming up anomaly detection...");
        return result;
    }

    prev = state.anomaly_last_result;
    // Use PREVIOUS baseline/stddev for scoring
    float prev_gas_baseline  = prev.gas_baseline;
    float prev_gas_stddev  = prev.gas_stddev;

    // Residuals against PREVIOUS baseline
    float gas_residual  = sample.gas_r - prev_gas_baseline;

    // Raw z-scores computed with PREVIOUS baseline/stddev
    float gas_z  = (sample.gas_r - prev_gas_baseline) / max(prev_gas_stddev, 10.0f);

    // Directional scores for wildfire detection
    result.gas_score  = directionalGasScore(gas_z);

    // Update baseline: freeze baseline if anomaly is detected;
    if (result.gas_score < GAS_WARNING_THRESHOLD || state.an_state == ANOMALY_WARMUP) { 
        result.gas_baseline  = gas_filter(sample.gas_r);
        // Update stddev using residuals against PREVIOUS baseline
        result.gas_stddev  = sqrtf(gas_variance_filter(gas_residual * gas_residual));

    } else {
        result.gas_baseline = prev_gas_baseline; // Keep previous baseline
        result.gas_stddev = prev_gas_stddev; // Keep previous stddev    
        Serial.println("Baseline frozen due to anomaly suspicioun.");
    }

    state.anomaly_last_proc_sample = sample;
    state.anomaly_last_result = result;
    return result;
}

void printAnomalyResults(anomaly_result_t result){
    Serial.printf("Gas Baseline: %.2f, Gas Stddev: %.2f, Gas Score: %.2f\n", 
        result.gas_baseline,
        result.gas_stddev,
        result.gas_score
    );
}