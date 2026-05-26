#pragma once

#include <Arduino.h>
#include <cstdint>

#include "WedsSensorSample.h"
#include "WedsTypes.h"

/**
 * @brief Represents the result of an anomaly detection evaluation.
 */
struct WedsAnomalyResult {
    float gas_baseline;    /**< The moving baseline of the gas resistance. */
    float gas_stddev;      /**< The moving standard deviation of the gas resistance. */
    float gas_score;       /**< The computed anomaly score for the gas resistance. */
    uint8_t detection_state; /**< The discrete detection state (e.g., normal or alert). */
    uint32_t samples_seen; /**< The total number of samples processed so far. */
    bool warming_up;       /**< Indicates if the detector is still in the warmup phase. */
};

struct WedsAnomalyEmaState {
    bool initialized;
    float value;
};

/**
 * @brief Anomaly detector that evaluates sensor samples to detect abnormal gas levels.
 */
class WedsAnomalyDetector {
public:
    /**
     * @brief Constructs a new WedsAnomalyDetector instance.
     */
    WedsAnomalyDetector();

    /**
     * @brief Initializes or restores RTC-retained anomaly detector state.
     */
    void begin();

    /**
     * @brief Updates the detector with a new sensor sample.
     * @param sample The latest sensor sample to process.
     * @return The updated anomaly result.
     */
    WedsAnomalyResult update(const WedsSensorSample& sample);

    /**
     * @brief Gets the most recent anomaly result.
     * @return A constant reference to the last computed WedsAnomalyResult.
     */
    const WedsAnomalyResult& lastResult() const;

    /**
     * @brief Gets the total number of samples processed.
     * @return The number of samples seen.
     */
    uint32_t samplesSeen() const;

    /**
     * @brief Checks if the detector is currently in the warmup phase.
     * @return True if in warmup, false otherwise.
     */
    bool isWarmup() const;

private:
    /**
     * @brief Exponential Moving Average (EMA) filter.
     * @tparam K The smoothing factor parameter (alpha = 1 / 2^K).
     */
    template <uint8_t K>
    class EmaFilter {
    public:
        /**
         * @brief Updates the filter with a new input value.
         * @param input The new input value to process.
         * @return The updated filtered value.
         */
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

        WedsAnomalyEmaState state() const {
            return {initialized_, state_};
        }

        void restore(const WedsAnomalyEmaState& state) {
            initialized_ = state.initialized;
            state_ = state.value;
        }

    private:
        bool initialized_ = false;
        float state_ = 0.0f;
    };

    static float positivePart(float value);
    static float negativePart(float value);
    static float directionalGasScore(float gas_z);

    bool started_;                   /**< Flag indicating if the detector has started processing. */
    uint32_t samples_seen_;          /**< Count of total processed samples. */
    WedsAnomalyResult last_result_;  /**< The most recently computed result. */
    EmaFilter<4> gas_filter_;        /**< Filter for computing the gas resistance baseline. */
    EmaFilter<4> gas_variance_filter_; /**< Filter for computing the gas resistance variance. */
};

/**
 * @brief Prints the anomaly results to the standard output.
 * @param result The anomaly result to print.
 */
void printAnomalyResults(const WedsAnomalyResult& result);
