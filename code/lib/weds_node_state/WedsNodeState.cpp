#include "WedsNodeState.h"
#include "WedsNodeConfig.h"

#include <esp_sleep.h>

namespace {

constexpr uint32_t WEDS_NODE_RTC_MAGIC = 0x574E4453UL; // "WNDS"
constexpr uint16_t WEDS_NODE_RTC_VERSION = 1;

struct WedsRtcNodeState {
  uint32_t magic;
  uint16_t version;
  uint32_t elapsed_sec;
  bool alert_mode_active;
  uint32_t alert_mode_until_sec;
  uint32_t alert_source_node_id;
  uint32_t alert_sampling_interval_sec;
  uint32_t sleep_duration_sec;
};

RTC_DATA_ATTR WedsRtcNodeState rtc_node_state;

void resetRtcNodeState() {
  rtc_node_state.magic = WEDS_NODE_RTC_MAGIC;
  rtc_node_state.version = WEDS_NODE_RTC_VERSION;
  rtc_node_state.elapsed_sec = 0;
  rtc_node_state.alert_mode_active = false;
  rtc_node_state.alert_mode_until_sec = 0;
  rtc_node_state.alert_source_node_id = 0;
  rtc_node_state.alert_sampling_interval_sec =
      WEDS_NODE_ALERT_MODE_SAMPLING_INTERVAL_SEC;
  rtc_node_state.sleep_duration_sec = WEDS_SLEEP_SEC_RISK_LOW;
}

bool rtcNodeStateValid() {
  return rtc_node_state.magic == WEDS_NODE_RTC_MAGIC &&
         rtc_node_state.version == WEDS_NODE_RTC_VERSION;
}

} // namespace

WedsNodeState::WedsNodeState() : node_id_(0), wake_start_ms_(0) {}

void WedsNodeState::begin(uint32_t node_id) {
  node_id_ = node_id;
  wake_start_ms_ = millis();

  const bool woke_from_deep_sleep =
      esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED;

  if (!woke_from_deep_sleep || !rtcNodeStateValid()) {
    resetRtcNodeState();
  }

  refreshAlertModeExpiry();
}

WedsNodeStatusPayload
WedsNodeState::buildPayload(const WedsSensorSample &sample,
                            const WedsAnomalyResult &anomaly,
                            const WedsRiskResult &risk) const {
  WedsNodeStatusPayload payload{};
  payload.timestamp_s = get_current_time();
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

void WedsNodeState::update_alert_mode(const WedsAnomalyResult &anomaly) {
  if (anomaly.detection_state == WEDS_DETECTION_ALERT) {
    activateAlertMode(node_id_, WEDS_NODE_ALERT_MODE_DURATION_SEC,
                      WEDS_NODE_ALERT_MODE_SAMPLING_INTERVAL_SEC);
    return;
  }

  refreshAlertModeExpiry();
}

uint32_t WedsNodeState::get_current_time() const {
  return rtc_node_state.elapsed_sec + ((millis() - wake_start_ms_) / 1000U);
}

void WedsNodeState::setSleepDuration(const WedsRiskResult &risk) {
  refreshAlertModeExpiry();

  if (rtc_node_state.alert_mode_active) {
    rtc_node_state.sleep_duration_sec =
        rtc_node_state.alert_sampling_interval_sec > 0
            ? rtc_node_state.alert_sampling_interval_sec
            : WEDS_NODE_ALERT_MODE_SAMPLING_INTERVAL_SEC;
  } else if (risk.score >= WEDS_RISK_THRESHOLD_MED) {
    rtc_node_state.sleep_duration_sec = WEDS_SLEEP_SEC_RISK_HIGH;
  } else if (risk.score >= WEDS_RISK_THRESHOLD_LOW) {
    rtc_node_state.sleep_duration_sec = WEDS_SLEEP_SEC_RISK_MED;
  } else {
    rtc_node_state.sleep_duration_sec = WEDS_SLEEP_SEC_RISK_LOW;
  }
}

uint32_t WedsNodeState::sleepDurationSec() const {
  return rtc_node_state.sleep_duration_sec;
}

void WedsNodeState::prepareForDeepSleep(uint32_t sleep_sec) {
  rtc_node_state.elapsed_sec = get_current_time() + sleep_sec;
  wake_start_ms_ = millis();
}

void WedsNodeState::applyAlertModeCommand(
    const WedsAlertModeEnablePayload &command) {
  activateAlertMode(command.alert_source_node_id, command.duration_sec,
                    command.sampling_interval_sec);
}

bool WedsNodeState::alertModeActive() const {
  return rtc_node_state.alert_mode_active &&
         get_current_time() < rtc_node_state.alert_mode_until_sec;
}

uint32_t WedsNodeState::nodeId() const { return node_id_; }

void WedsNodeState::activateAlertMode(uint32_t source_node_id,
                                      uint32_t duration_sec,
                                      uint32_t sampling_interval_sec) {
  rtc_node_state.alert_mode_active = duration_sec > 0;
  rtc_node_state.alert_mode_until_sec = get_current_time() + duration_sec;
  rtc_node_state.alert_source_node_id = source_node_id;
  rtc_node_state.alert_sampling_interval_sec =
      sampling_interval_sec > 0 ? sampling_interval_sec
                                : WEDS_NODE_ALERT_MODE_SAMPLING_INTERVAL_SEC;
}

void WedsNodeState::refreshAlertModeExpiry() {
  if (rtc_node_state.alert_mode_active &&
      get_current_time() >= rtc_node_state.alert_mode_until_sec) {
    rtc_node_state.alert_mode_active = false;
    rtc_node_state.alert_mode_until_sec = 0;
    rtc_node_state.alert_source_node_id = 0;
    rtc_node_state.alert_sampling_interval_sec =
        WEDS_NODE_ALERT_MODE_SAMPLING_INTERVAL_SEC;
  }
}

float WedsNodeState::readBatteryLevel() const {
  // Set the ADC resolution to 12 bits
  analogReadResolution(12);

  // Set control pin to OUTPUT and HIGH to enable battery voltage reading on Heltec V3.2/V4
  pinMode(WEDS_NODE_HELTEC_VBAT_CTRL_PIN, OUTPUT);
  digitalWrite(WEDS_NODE_HELTEC_VBAT_CTRL_PIN, HIGH);
  delay(10); // Brief delay for voltage to stabilize

  // Read voltage in millivolts
  int analogMilliVolts = analogReadMilliVolts(WEDS_NODE_HELTEC_VBAT_ADC_PIN);

  // Set control pin back to INPUT (low power state)
  digitalWrite(WEDS_NODE_HELTEC_VBAT_CTRL_PIN, LOW);
  pinMode(WEDS_NODE_HELTEC_VBAT_CTRL_PIN, INPUT);

  // Calculate battery voltage (factor is 4.9 for 390k/100k divider)
  float battery_v = (analogMilliVolts * 4.9f) / 1000.0f;

  if (battery_v <= WEDS_NODE_BATTERY_EMPTY_V) {
    return 0.0f;
  }
  if (battery_v >= WEDS_NODE_BATTERY_FULL_V) {
    return 100.0f;
  }

  float battery_pct = (battery_v - WEDS_NODE_BATTERY_EMPTY_V) *
                      WEDS_NODE_BATTERY_PERCENT_PER_V;

  Serial.printf("[NODE] Raw pin mV: %d, battery_v: %.2f V, percent: %.1f%%\n",
                analogMilliVolts, battery_v, battery_pct);

  return battery_pct;
}
