#include "WedsNodeState.h"
#include "WedsNodeConfig.h"

WedsNodeState::WedsNodeState()
    : node_id_(0),
      sleep_enabled_(true),
      alert_mode_active_(false),
      alert_mode_until_ms_(0),
      normal_sample_interval_ms_(WEDS_NODE_DEFAULT_SAMPLE_INTERVAL_MS),
      active_sample_interval_ms_(WEDS_NODE_DEFAULT_SAMPLE_INTERVAL_MS),
      alert_source_node_id_(0) {}

void WedsNodeState::begin(uint32_t node_id) {
    node_id_ = node_id;
}

WedsNodeStatusPayload WedsNodeState::buildPayload(
    const WedsSensorSample& sample,
    const WedsAnomalyResult& anomaly,
    const WedsRiskResult& risk
) const {
    WedsNodeStatusPayload payload{};
    payload.timestamp_s = millis() / 1000;
    payload.temperature = sample.temperature;
    payload.humidity = sample.humidity;
    payload.pressure = sample.pressure;
    payload.gas_resistance = sample.gas_resistance;
    payload.battery_level = readBatteryLevel();
    payload.anomaly_state = anomaly.detection_state;
    payload.anomaly_score = anomaly.gas_score;
    payload.risk_state = risk.detection_state;
    payload.risk_score = risk.score;
    return payload;
}

void WedsNodeState::setSleepEnabled(bool enabled) {
    sleep_enabled_ = enabled;
}

bool WedsNodeState::sleepEnabled() const {
    return sleep_enabled_;
}

void WedsNodeState::applyAlertModeCommand(const WedsAlertModeEnablePayload& command) {
    alert_mode_active_ = true;
    alert_source_node_id_ = command.alert_source_node_id;
    alert_mode_until_ms_ = millis() + static_cast<uint32_t>(command.duration_sec) * 1000UL;

    if (command.sampling_interval_sec > 0) {
        active_sample_interval_ms_ =
            static_cast<uint32_t>(command.sampling_interval_sec) * 1000UL;
    }

    sleep_enabled_ = false;

    Serial.print("[NODE_STATE] Alert mode enabled by node=");
    Serial.print(alert_source_node_id_);
    Serial.print(" interval_ms=");
    Serial.print(active_sample_interval_ms_);
    Serial.print(" until_ms=");
    Serial.println(alert_mode_until_ms_);
}

void WedsNodeState::refreshAlertMode() {
    if (!alert_mode_active_) {
        return;
    }

    if (static_cast<int32_t>(millis() - alert_mode_until_ms_) < 0) {
        return;
    }

    alert_mode_active_ = false;
    alert_source_node_id_ = 0;
    active_sample_interval_ms_ = normal_sample_interval_ms_;
    sleep_enabled_ = true;
    Serial.println("[NODE_STATE] Alert mode expired");
}

bool WedsNodeState::alertModeActive() const {
    return alert_mode_active_;
}

uint32_t WedsNodeState::sampleIntervalMs() const {
    return alert_mode_active_ ? active_sample_interval_ms_ : normal_sample_interval_ms_;
}

uint32_t WedsNodeState::nodeId() const {
    return node_id_;
}

float WedsNodeState::readBatteryLevel() const {
    pinMode(WEDS_NODE_HELTEC_VBAT_CTRL_PIN, OUTPUT);
    digitalWrite(WEDS_NODE_HELTEC_VBAT_CTRL_PIN, LOW);
    delay(5);

    const float battery_v =
        static_cast<float>(analogRead(WEDS_NODE_HELTEC_VBAT_ADC_PIN)) *
        WEDS_NODE_HELTEC_ADC_TO_VBAT;

    pinMode(WEDS_NODE_HELTEC_VBAT_CTRL_PIN, INPUT);

    if (battery_v <= WEDS_NODE_BATTERY_EMPTY_V) {
        return 0.0f;
    }
    if (battery_v >= WEDS_NODE_BATTERY_FULL_V) {
        return 100.0f;
    }
    return (battery_v - WEDS_NODE_BATTERY_EMPTY_V) * WEDS_NODE_BATTERY_PERCENT_PER_V;
}
