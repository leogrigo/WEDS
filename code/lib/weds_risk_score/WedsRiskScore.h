#pragma once

#include <cstdint>
#include "WedsSensorSample.h"
#include "WedsTypes.h"

#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

/**
 * @brief Represents the evaluated risk output from the TinyML model.
 */
struct WedsRiskResult {
    uint8_t detection_state; ///< The discrete alert level (e.g., Normal, Alert) based on the score.
    float score;             ///< The raw probability of a fire event, ranging from 0.0 (no risk) to 1.0 (extreme risk).
};

/**
 * @brief A memory-efficient structure representing a 24-hour aggregation of sensor data.
 * Used within a ring buffer to calculate multi-day rolling averages without storing high-frequency arrays.
 */
struct DailyBucket {
    uint32_t day_id;      ///< Absolute day identifier (UNIX timestamp / 86400).
    float temp_sum;       ///< Accumulated temperature readings for this day.
    float rh_sum;         ///< Accumulated humidity readings for this day.
    uint16_t sample_count;///< Number of readings aggregated in this bucket.
};

/**
 * @brief Manages the execution and state of the TinyML fire risk prediction model.
 *
 * This class maintains a 7-day rolling history of environmental data to compute
 * the required time-series features (averages and deltas) before feeding them into
 * the quantized TensorFlow Lite neural network. The rolling history is stored in
 * RTC-retained memory so it survives hardware deep-sleep resets.
 */
class WedsRiskScoreCalculator {
public:
    static const int kHistorySize = 7;                /**< Number of days in the rolling history buffer. */

    /**
     * @brief Allocates tensor memory and initializes the neural network interpreter.
     * @return true if the model loaded and memory allocated successfully, false otherwise.
     */
    bool begin();

    /**
     * @brief Processes a new sensor reading, updates the historical rolling buffers, and runs an inference pass.
     * @param sample The latest environmental readings containing temperature, humidity, pressure, and timestamp.
     * @return A WedsRiskResult containing the computed probability score and alert state.
     */
    WedsRiskResult update(const WedsSensorSample& sample);

private:
    const tflite::Model* model_ = nullptr;            /**< Pointer to the parsed TFLite model. */
    tflite::MicroInterpreter* interpreter_ = nullptr; /**< Interpreter for running inference. */
    TfLiteTensor* input_ = nullptr;                   /**< Pointer to the model's input tensor. */
    TfLiteTensor* output_ = nullptr;                  /**< Pointer to the model's output tensor. */

    static const int kTensorArenaSize = 15 * 1024;    /**< Memory arena size for TFLite allocations. */
    uint8_t tensor_arena_[kTensorArenaSize];          /**< Buffer for the tensor arena. */

    static const int kNumFeatures = 7;                /**< Number of input features to the model. */
    float scaler_means_[kNumFeatures];                /**< Means for feature scaling. */
    float scaler_scales_[kNumFeatures];               /**< Scales for feature scaling. */

};

