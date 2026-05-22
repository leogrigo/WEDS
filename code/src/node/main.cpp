#include <Arduino.h>
#include <esp_sleep.h>

#include "WedsNodeComm.h"
#include "WedsNodeState.h"
#include "WedsAnomalyDetector.h"
#include "WedsNodeSimulation.h"
#include "WedsRiskScore.h"
#include "WedsSensors.h"
#include "WedsNodeConfig.h"

namespace {

WedsNodeComm nodeComm;
WedsNodeState nodeState;
WedsAnomalyDetector anomalyDetector;
WedsRiskScoreCalculator riskScoreCalculator;

/**
 * @brief Halts the system and blinks an error code if a critical boot failure occurs.
 */
void fatalError(const char* message) {
    Serial.print("[NODE_FATAL] ");
    Serial.println(message);

    for (;;) {
        delay(WEDS_NODE_ERROR_TASK_DELAY_MS);
    }
}

#ifdef WEDS_LEGACY_SENSORS
WedsSensorSample readSensors() {
    if (WEDS_NODE_SENSING_MODE == WEDS_NODE_ENVIRONMENT_SENSING) {
        return weds_read_environment_sample();
    }

    return weds_read_simulated_sample();
}
#else

WedsSensorSample readSensors() {
    if (WEDS_NODE_SENSING_MODE == WEDS_NODE_ENVIRONMENT_SENSING) {
        return weds_read_environment_sample();
    }

    return weds_read_simulated_sample();
}

#endif

bool isAlertPayload(const WedsNodeStatusPayload& payload) {
    return payload.anomaly_state == WEDS_DETECTION_ALERT ||
        payload.risk_state == WEDS_DETECTION_ALERT;
}

bool sendPayload(const WedsNodeStatusPayload& payload) {
    const bool sent = isAlertPayload(payload)
        ? nodeComm.sendAlert(payload)
        : nodeComm.sendStatus(payload);

    return sent;
}

/**
 * @brief Determines the deep sleep duration in seconds based on the fire risk score.
 * @param risk_score The raw probability output from the TinyML model (0.0 – 1.0).
 * @return Sleep duration in seconds as defined by the dynamic duty cycle configuration.
 */
uint32_t sleepDurationSec(float risk_score) {
    if (risk_score >= WEDS_RISK_THRESHOLD_MED) {
        return WEDS_SLEEP_SEC_RISK_HIGH;
    }
    if (risk_score >= WEDS_RISK_THRESHOLD_LOW) {
        return WEDS_SLEEP_SEC_RISK_MED;
    }
    return WEDS_SLEEP_SEC_RISK_LOW;
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(WEDS_NODE_BOOT_DELAY_MS);

    Serial.println();
    Serial.println("=== WEDS Node Firmware ===");

    if (WEDS_NODE_SENSING_MODE == WEDS_NODE_ENVIRONMENT_SENSING) {
        weds_sensors_begin();
    }

    randomSeed(micros());

    const bool commReady = nodeComm.begin();
    if (!commReady) {
        fatalError("Node communication init failed");
    }

    nodeState.begin(nodeComm.getNodeId());

    Serial.println("[NODE] Initializing TinyML Model...");
    if (!riskScoreCalculator.begin()) {
        fatalError("Failed to initialize Risk Score Model");
    }

    const uint32_t wake_start_ms = millis();

    WedsSensorSample sample = readSensors();

    if (rtc_virtual_timestamp == 0) {
        rtc_virtual_timestamp = 86400U;
    }
    sample.timestamp = rtc_virtual_timestamp;

    if (!sample.valid || isnan(sample.temperature) || isnan(sample.humidity)) {
        Serial.println("[NODE_WARN] Sensor fault detected (NaN or invalid read) — retrying in 5s");
        Serial.flush();
        esp_deep_sleep(5000000ULL);
        return;
    }

    const WedsAnomalyResult anomaly = anomalyDetector.update(sample);
    printAnomalyResults(anomaly);

    const WedsRiskResult risk = riskScoreCalculator.update(sample);
    Serial.printf("[NODE] Fire Risk Score: %.2f%%\n", risk.score * 100.0f);

    const WedsNodeStatusPayload payload = nodeState.buildPayload(sample, anomaly, risk);

    const uint32_t rx_window_start_ms = millis();
    while ((millis() - rx_window_start_ms) < WEDS_NODE_MIN_RX_WINDOW_MS) {
        WedsAlertModeEnablePayload command{};
        if (nodeComm.pollAlertModeEnable(command, WEDS_NODE_RX_POLL_CHUNK_MS)) {
            nodeState.applyAlertModeCommand(command);
        }
    }

    const bool sent = sendPayload(payload);
    if (!sent) {
        Serial.println("[NODE] Failed to send payload to gateway");
    }

    nodeComm.sleepRadio();

    const uint32_t sleep_sec = sleepDurationSec(risk.score);
    const uint32_t awake_sec = (millis() - wake_start_ms) / 1000U;

    rtc_virtual_timestamp += awake_sec + sleep_sec;

    Serial.printf("[NODE] Sleeping for %lu s (risk=%.2f)\n",
                  (unsigned long)sleep_sec, risk.score);
    Serial.flush();

    esp_deep_sleep((uint64_t)sleep_sec * 1000000ULL);
}

void loop() {}