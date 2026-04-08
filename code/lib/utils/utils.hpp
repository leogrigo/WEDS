#pragma once
#include <Arduino.h>

constexpr float PI_F = 3.14159265f;

float sampleNormal(float mean, float stddev);
float sampleUniform(float min_val, float max_val);
float computeZScore(float sample, float baseline, float stddev);
float positivePart(float x);
float directionalTempScore(float z);
float directionalHumScore(float z);
float directionalGasScore(float z);

// Exponential Moving Average Filter (EMA)
template <uint8_t K>
class EMA {
  public:
    float operator()(float input) {
        if (!initialized) {
            state = input;
            initialized = true;
            return input;
        }

        constexpr float alpha = 1.0f / (1 << K);
        state += alpha * (input - state);
        return state;
    }

  private:
    bool initialized = false;
    float state = 0.0f;
};
