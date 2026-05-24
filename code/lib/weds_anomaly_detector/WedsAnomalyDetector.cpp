#include "WedsAnomalyDetector.h"

#include <esp_sleep.h>

namespace {

constexpr float WEDS_GAS_WARNING_THRESHOLD = 1.5f;
constexpr uint8_t WEDS_ANOMALY_WARMUP_SAMPLES = 20;
constexpr float WEDS_ANOMALY_MIN_GAS_STDDEV = 10.0f;
constexpr uint32_t WEDS_ANOMALY_RTC_MAGIC = 0x57414E4FUL;  // "WANO"
constexpr uint16_t WEDS_ANOMALY_RTC_VERSION = 1;

struct WedsRtcAnomalyState {
    uint32_t magic;
    uint16_t version;
    bool started;
    uint32_t samples_seen;
    WedsAnomalyResult last_result;
    WedsAnomalyEmaState gas_filter;
    WedsAnomalyEmaState gas_variance_filter;
};

RTC_DATA_ATTR WedsRtcAnomalyState rtc_anomaly_state;

bool rtcAnomalyStateValid() {
    return rtc_anomaly_state.magic == WEDS_ANOMALY_RTC_MAGIC &&
        rtc_anomaly_state.version == WEDS_ANOMALY_RTC_VERSION;
}

void clearRtcAnomalyState() {
    rtc_anomaly_state.magic = 0;
    rtc_anomaly_state.version = 0;
    rtc_anomaly_state.started = false;
    rtc_anomaly_state.samples_seen = 0;
    rtc_anomaly_state.last_result = {};
    rtc_anomaly_state.gas_filter = {};
    rtc_anomaly_state.gas_variance_filter = {};
}

}  // namespace

WedsAnomalyDetector::WedsAnomalyDetector()
    : started_(false),
      samples_seen_(0),
      last_result_{} {
    last_result_.detection_state = WEDS_DETECTION_NORMAL;
    last_result_.warming_up = true;
}

void WedsAnomalyDetector::begin() {
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
        clearRtcAnomalyState();
    }

    if (rtcAnomalyStateValid()) {
        started_ = rtc_anomaly_state.started;
        samples_seen_ = rtc_anomaly_state.samples_seen;
        last_result_ = rtc_anomaly_state.last_result;
        gas_filter_.restore(rtc_anomaly_state.gas_filter);
        gas_variance_filter_.restore(rtc_anomaly_state.gas_variance_filter);
    } else {
        started_ = false;
        samples_seen_ = 0;
        last_result_ = {};
        last_result_.detection_state = WEDS_DETECTION_NORMAL;
        last_result_.warming_up = true;
    }
}

WedsAnomalyResult WedsAnomalyDetector::update(const WedsSensorSample& sample) {
    WedsAnomalyResult result{};
    samples_seen_++;

    if (!started_) {
        started_ = true;
        result.gas_baseline = gas_filter_.update(sample.gas_resistance);
        result.gas_stddev = 0.0f;
        result.gas_score = 0.0f;
        result.detection_state = WEDS_DETECTION_NORMAL;
        result.samples_seen = samples_seen_;
        result.warming_up = true;
        last_result_ = result;
        rtc_anomaly_state.magic = WEDS_ANOMALY_RTC_MAGIC;
        rtc_anomaly_state.version = WEDS_ANOMALY_RTC_VERSION;
        rtc_anomaly_state.started = started_;
        rtc_anomaly_state.samples_seen = samples_seen_;
        rtc_anomaly_state.last_result = last_result_;
        rtc_anomaly_state.gas_filter = gas_filter_.state();
        rtc_anomaly_state.gas_variance_filter = gas_variance_filter_.state();
        Serial.println("[ANOMALY] Warmup started");
        return result;
    }

    const bool warming_up = samples_seen_ <= WEDS_ANOMALY_WARMUP_SAMPLES;
    const float prev_gas_baseline = last_result_.gas_baseline;
    const float prev_gas_stddev = last_result_.gas_stddev;
    const float gas_residual = sample.gas_resistance - prev_gas_baseline;
    const float gas_z = gas_residual / max(prev_gas_stddev, WEDS_ANOMALY_MIN_GAS_STDDEV);

    result.gas_score = directionalGasScore(gas_z);
    result.detection_state =
        (!warming_up && result.gas_score > WEDS_GAS_WARNING_THRESHOLD)
            ? WEDS_DETECTION_ALERT
            : WEDS_DETECTION_NORMAL;

    if (result.detection_state == WEDS_DETECTION_NORMAL || warming_up) {
        result.gas_baseline = gas_filter_.update(sample.gas_resistance);
        result.gas_stddev = sqrtf(gas_variance_filter_.update(gas_residual * gas_residual));
    } else {
        result.gas_baseline = prev_gas_baseline;
        result.gas_stddev = prev_gas_stddev;
        Serial.println("[ANOMALY] Baseline frozen while alert is active");
    }

    result.samples_seen = samples_seen_;
    result.warming_up = warming_up;
    last_result_ = result;
    rtc_anomaly_state.magic = WEDS_ANOMALY_RTC_MAGIC;
    rtc_anomaly_state.version = WEDS_ANOMALY_RTC_VERSION;
    rtc_anomaly_state.started = started_;
    rtc_anomaly_state.samples_seen = samples_seen_;
    rtc_anomaly_state.last_result = last_result_;
    rtc_anomaly_state.gas_filter = gas_filter_.state();
    rtc_anomaly_state.gas_variance_filter = gas_variance_filter_.state();
    return result;
}

const WedsAnomalyResult& WedsAnomalyDetector::lastResult() const {
    return last_result_;
}

uint32_t WedsAnomalyDetector::samplesSeen() const {
    return samples_seen_;
}

bool WedsAnomalyDetector::isWarmup() const {
    return !started_ || samples_seen_ <= WEDS_ANOMALY_WARMUP_SAMPLES;
}

float WedsAnomalyDetector::positivePart(float value) {
    return value > 0.0f ? value : 0.0f;
}

float WedsAnomalyDetector::negativePart(float value) {
    return value < 0.0f ? value*-1.0f : 0.0f;
}

float WedsAnomalyDetector::directionalGasScore(float gas_z) {
    return negativePart(gas_z);
}

void printAnomalyResults(const WedsAnomalyResult& result) {
    Serial.printf(
        "[ANOMALY] state=%s score=%.2f baseline=%.0f stddev=%.1f samples=%lu warmup=%s\n",
        result.detection_state == WEDS_DETECTION_ALERT ? "ALERT" : "NORMAL",
        result.gas_score,
        result.gas_baseline,
        result.gas_stddev,
        static_cast<unsigned long>(result.samples_seen),
        result.warming_up ? "yes" : "no"
    );
}
