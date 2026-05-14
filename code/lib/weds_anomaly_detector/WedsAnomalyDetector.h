#pragma once

#include <Arduino.h>
#include <cstdint>

#include "WedsSensorSample.h"
#include "WedsTypes.h"

struct WedsAnomalyResult {
    float gas_baseline;
    float gas_stddev;
    float gas_score;
    uint8_t detection_state;
    uint32_t samples_seen;
    bool warming_up;
};

class WedsAnomalyDetector {
public:
    WedsAnomalyDetector();

    WedsAnomalyResult update(const WedsSensorSample& sample);
    const WedsAnomalyResult& lastResult() const;
    uint32_t samplesSeen() const;
    bool isWarmup() const;

private:
    template <uint8_t K>
    class EmaFilter {
    public:
        float update(float input) {
            if (!initialized_) {
                state_ = input;
                initialized_ = true;
                return input;
            }

            constexpr float alpha = 1.0f / (1 << K);
            state_ += alpha * (input - state_);
            return state_;
        }

    private:
        bool initialized_ = false;
        float state_ = 0.0f;
    };

    static float positivePart(float value);
    static float directionalGasScore(float gas_z);

    bool started_;
    uint32_t samples_seen_;
    WedsAnomalyResult last_result_;
    EmaFilter<4> gas_filter_;
    EmaFilter<4> gas_variance_filter_;
};

void printAnomalyResults(const WedsAnomalyResult& result);
