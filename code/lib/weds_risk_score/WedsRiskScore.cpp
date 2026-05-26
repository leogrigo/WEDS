#include "WedsRiskScore.h"
#include "WedsNodeConfig.h"
#include "esp_heap_caps.h"
#include "model_data_v6f.h"
#include "soc/timer_group_reg.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include <Arduino.h>
#include <cmath>
#include <esp_task_wdt.h>
#include <fenv.h>

RTC_DATA_ATTR static DailyBucket
    rtc_history[WedsRiskScoreCalculator::kHistorySize];

bool WedsRiskScoreCalculator::begin() {
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
    for (int i = 0; i < kHistorySize; ++i) {
      rtc_history[i] = {};
    }
  }

  const float means[kNumFeatures] = {18.61777f, 63.66107f, 95955.32f, 18.68431f,
                                     63.42773f, -0.01751f, 0.09243f};
  const float scales[kNumFeatures] = {6.07850f,  15.99210f, 4519.777f, 5.72370f,
                                      13.88332f, 1.76610f,  9.05942f};

  for (int i = 0; i < kNumFeatures; i++) {
    scaler_means_[i] = means[i];
    scaler_scales_[i] = scales[i];
  }

  if (!tensor_arena_) {
    // dynamic allocation removed.
  }
  Serial.printf("[DEBUG] Tensor arena address: %p, is aligned: %d\n",
                tensor_arena_, ((uintptr_t)tensor_arena_ % 16) == 0);

  model_ = tflite::GetModel(g_model_data);
  if (model_->version() != TFLITE_SCHEMA_VERSION)
    return false;

  static tflite::MicroErrorReporter micro_error_reporter;
  static tflite::AllOpsResolver resolver;
  static tflite::MicroInterpreter static_interpreter(
      model_, resolver, tensor_arena_, kTensorArenaSize, &micro_error_reporter);
  interpreter_ = &static_interpreter;

  if (interpreter_->AllocateTensors() != kTfLiteOk)
    return false;

  input_ = interpreter_->input(0);
  output_ = interpreter_->output(0);
  return true;
}

WedsRiskResult WedsRiskScoreCalculator::update(const WedsSensorSample &sample) {
  WedsRiskResult result;
  result.score = 0.0f;
  result.detection_state = WEDS_DETECTION_NORMAL;

  if (WEDS_NODE_SKIP_RISK_INFERENCE) {
    result.score = WEDS_NODE_DEFAULT_RISK_SCORE;
    result.detection_state =
        result.score > 0.75f ? WEDS_DETECTION_ALERT : WEDS_DETECTION_NORMAL;
    return result;
  }

  if (!interpreter_)
    return result;

  const uint32_t model_timestamp =
      sample.timestamp == 0 ? 86400U : sample.timestamp;
  uint32_t current_day = model_timestamp / 86400;
  int bucket_idx = current_day % kHistorySize;

  if (rtc_history[bucket_idx].day_id != current_day) {
    rtc_history[bucket_idx].day_id = current_day;
    rtc_history[bucket_idx].temp_sum = 0.0f;
    rtc_history[bucket_idx].rh_sum = 0.0f;
    rtc_history[bucket_idx].sample_count = 0;
  }

  rtc_history[bucket_idx].temp_sum += sample.temperature;
  rtc_history[bucket_idx].rh_sum += sample.humidity;
  rtc_history[bucket_idx].sample_count++;

  float total_temp_avg = 0.0f;
  float total_rh_avg = 0.0f;
  int valid_days = 0;

  for (int i = 0; i < kHistorySize; i++) {
    if (rtc_history[i].sample_count > 0 &&
        (current_day - rtc_history[i].day_id < (uint32_t)kHistorySize)) {
      total_temp_avg += (rtc_history[i].temp_sum / rtc_history[i].sample_count);
      total_rh_avg += (rtc_history[i].rh_sum / rtc_history[i].sample_count);
      valid_days++;
    }
  }

  float avgw_temp =
      valid_days > 0 ? (total_temp_avg / valid_days) : sample.temperature;
  float avgw_rh =
      valid_days > 0 ? (total_rh_avg / valid_days) : sample.humidity;

  float temp_delta = 0.0f;
  float rh_delta = 0.0f;

  int yesterday_idx = (current_day + kHistorySize - 1) % kHistorySize;

  if (rtc_history[yesterday_idx].sample_count > 0 &&
      rtc_history[yesterday_idx].day_id == current_day - 1) {
    float today_temp_avg =
        rtc_history[bucket_idx].temp_sum / rtc_history[bucket_idx].sample_count;
    float today_rh_avg =
        rtc_history[bucket_idx].rh_sum / rtc_history[bucket_idx].sample_count;

    float yesterday_temp_avg = rtc_history[yesterday_idx].temp_sum /
                               rtc_history[yesterday_idx].sample_count;
    float yesterday_rh_avg = rtc_history[yesterday_idx].rh_sum /
                             rtc_history[yesterday_idx].sample_count;

    temp_delta = today_temp_avg - yesterday_temp_avg;
    rh_delta = today_rh_avg - yesterday_rh_avg;
  }

  float raw_features[kNumFeatures] = {
      sample.temperature, sample.humidity, sample.pressure, avgw_temp, avgw_rh,
      temp_delta,         rh_delta};

  Serial.println("[DEBUG] Packing input tensor...");
  Serial.printf("[DEBUG] Input type: %d, bytes: %d, dims->size: %d\n",
                input_->type, input_->bytes, input_->dims->size);
  for (int i = 0; i < input_->dims->size; i++) {
    Serial.printf("[DEBUG] dim[%d]: %d\n", i, input_->dims->data[i]);
  }

  if (input_->bytes < kNumFeatures) {
    Serial.printf(
        "[NODE_FATAL] Model expects %d bytes but we are packing %d!\n",
        input_->bytes, kNumFeatures);
    return result;
  }

  for (int i = 0; i < kNumFeatures; i++) {
    float scaled_val = (raw_features[i] - scaler_means_[i]) / scaler_scales_[i];

    // Prevent denormal floats from stalling the FPU
    if (std::fpclassify(scaled_val) == FP_SUBNORMAL) {
      scaled_val = 0.0f;
    }

    if (input_->type == kTfLiteFloat32) {
      input_->data.f[i] = scaled_val;
    } else if (input_->type == kTfLiteInt8) {
      int32_t quantized_val =
          (int32_t)round(scaled_val / input_->params.scale) +
          input_->params.zero_point;

      if (quantized_val < -128)
        quantized_val = -128;
      if (quantized_val > 127)
        quantized_val = 127;

      input_->data.int8[i] = (int8_t)quantized_val;
    } else {
      Serial.printf("[NODE_FATAL] Unsupported input tensor type: %d\n",
                    input_->type);
      return result;
    }
  }

  Serial.println("[DEBUG] Feeding WDT before Invoke...");

  Serial.println("[DEBUG] Starting TFLite Invoke...");
  uint32_t invoke_start = millis();
  TfLiteStatus invoke_status = interpreter_->Invoke();
  uint32_t invoke_end = millis();

  Serial.printf("[DEBUG] Invoke completed in %lu ms. Status: %d\n",
                invoke_end - invoke_start, invoke_status);

  if (invoke_status != kTfLiteOk)
    return result;

  Serial.println("[DEBUG] Unpacking output tensor...");
  if (output_->type == kTfLiteFloat32) {
    result.score = output_->data.f[0];
  } else if (output_->type == kTfLiteInt8) {
    result.score = (output_->data.int8[0] - output_->params.zero_point) *
                   output_->params.scale;
  } else {
    Serial.printf("[NODE_FATAL] Unsupported output tensor type: %d\n",
                  output_->type);
    return result;
  }

  if (result.score > 0.75f) {
    result.detection_state = WEDS_DETECTION_ALERT;
  }

  return result;
}
