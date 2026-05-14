#include "WedsAnomalyDetector.h"

namespace {

constexpr float WEDS_GAS_WARNING_THRESHOLD = 1.5f;
constexpr uint8_t WEDS_ANOMALY_WARMUP_SAMPLES = 20;
constexpr float WEDS_ANOMALY_MIN_GAS_STDDEV = 10.0f;

}  // namespace

WedsAnomalyDetector::WedsAnomalyDetector()
    : started_(false),
      samples_seen_(0),
      last_result_{} {
    last_result_.detection_state = WEDS_DETECTION_NORMAL;
    last_result_.warming_up = true;
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

float WedsAnomalyDetector::directionalGasScore(float gas_z) {
    return positivePart(gas_z);
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
