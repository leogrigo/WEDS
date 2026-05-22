#include "WedsRiskScore.h"
#include <Arduino.h>
#include "model_data_v6f.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"

RTC_DATA_ATTR static DailyBucket rtc_history[WedsRiskScoreCalculator::kHistorySize];
RTC_DATA_ATTR uint32_t           rtc_virtual_timestamp = 0;

bool WedsRiskScoreCalculator::begin() {
    const float means[kNumFeatures] = {18.61777f, 63.66107f, 95955.32f, 18.68431f, 63.42773f, -0.01751f, 0.09243f};
    const float scales[kNumFeatures] = {6.07850f, 15.99210f, 4519.777f, 5.72370f, 13.88332f, 1.76610f, 9.05942f};

    for (int i = 0; i < kNumFeatures; i++) {
        scaler_means_[i] = means[i];
        scaler_scales_[i] = scales[i];
    }

    model_ = tflite::GetModel(_content_drive_MyDrive_Models_WEDS___RiskScore_tiny_fire_risk_tmodel_quant_v6f_tflite);
    if (model_->version() != TFLITE_SCHEMA_VERSION) return false;

    static tflite::MicroErrorReporter micro_error_reporter;
    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter static_interpreter(
        model_, resolver, tensor_arena_, kTensorArenaSize, &micro_error_reporter);
    interpreter_ = &static_interpreter;

    if (interpreter_->AllocateTensors() != kTfLiteOk) return false;

    input_ = interpreter_->input(0);
    output_ = interpreter_->output(0);
    return true;
}

WedsRiskResult WedsRiskScoreCalculator::update(const WedsSensorSample& sample) {
    WedsRiskResult result;
    result.score = 0.0f;
    result.detection_state = WEDS_DETECTION_NORMAL;

    if (!interpreter_ || sample.timestamp == 0) return result;

    uint32_t current_day = sample.timestamp / 86400;
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
        if (rtc_history[i].sample_count > 0 && (current_day - rtc_history[i].day_id < (uint32_t)kHistorySize)) {
            total_temp_avg += (rtc_history[i].temp_sum / rtc_history[i].sample_count);
            total_rh_avg += (rtc_history[i].rh_sum / rtc_history[i].sample_count);
            valid_days++;
        }
    }

    float avgw_temp = valid_days > 0 ? (total_temp_avg / valid_days) : sample.temperature;
    float avgw_rh = valid_days > 0 ? (total_rh_avg / valid_days) : sample.humidity;

    float temp_delta = 0.0f;
    float rh_delta = 0.0f;

    int yesterday_idx = (current_day + kHistorySize - 1) % kHistorySize;

    if (rtc_history[yesterday_idx].sample_count > 0 && rtc_history[yesterday_idx].day_id == current_day - 1) {
        float today_temp_avg = rtc_history[bucket_idx].temp_sum / rtc_history[bucket_idx].sample_count;
        float today_rh_avg = rtc_history[bucket_idx].rh_sum / rtc_history[bucket_idx].sample_count;

        float yesterday_temp_avg = rtc_history[yesterday_idx].temp_sum / rtc_history[yesterday_idx].sample_count;
        float yesterday_rh_avg = rtc_history[yesterday_idx].rh_sum / rtc_history[yesterday_idx].sample_count;

        temp_delta = today_temp_avg - yesterday_temp_avg;
        rh_delta = today_rh_avg - yesterday_rh_avg;
    }

    float raw_features[kNumFeatures] = {
        sample.temperature,
        sample.humidity,
        sample.pressure,
        avgw_temp,
        avgw_rh,
        temp_delta,
        rh_delta
    };

    for (int i = 0; i < kNumFeatures; i++) {
        float scaled_val = (raw_features[i] - scaler_means_[i]) / scaler_scales_[i];
        int8_t quantized_val = (int8_t)((scaled_val / input_->params.scale) + input_->params.zero_point);
        input_->data.int8[i] = quantized_val;
    }

    if (interpreter_->Invoke() != kTfLiteOk) return result;

    result.score = (output_->data.int8[0] - output_->params.zero_point) * output_->params.scale;

    if (result.score > 0.75f) {
        result.detection_state = WEDS_DETECTION_ALERT;
    }

    return result;
}