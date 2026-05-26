#pragma once

#include <Arduino.h>

#include "WedsProtocol.h"
#include "WedsAnomalyDetector.h"
#include "WedsRiskScore.h"
#include "WedsSensorSample.h"

/**
 * @class WedsNodeState
 * @brief Manages the operational state and configuration of a sensor node.
 */
class WedsNodeState {
public:
    /**
     * @brief Constructs a new WedsNodeState object with default values.
     */
    WedsNodeState();

    /**
     * @brief Initializes the node state.
     * @param node_id The unique identifier for this node.
     */
    void begin(uint32_t node_id);

    /**
     * @brief Builds a status payload for transmission.
     * @param sample The latest sensor sample.
     * @param anomaly The anomaly detection result.
     * @param risk The risk score result.
     * @return WedsNodeStatusPayload The constructed payload.
     */
    WedsNodeStatusPayload buildPayload(
        const WedsSensorSample& sample,
        const WedsAnomalyResult& anomaly,
        const WedsRiskResult& risk
    ) const;

    /**
     * @brief Updates alert mode using the latest anomaly result.
     * @param anomaly The latest anomaly detection result.
     */
    void update_alert_mode(const WedsAnomalyResult& anomaly);

    /**
     * @brief Gets elapsed time since the last non-deep-sleep reset.
     * @return uint32_t Elapsed time in seconds.
     */
    uint32_t get_current_time() const;

    /**
     * @brief Chooses the next deep sleep duration from alert/risk state.
     * @param risk The latest fire risk result.
     */
    void setSleepDuration(const WedsRiskResult& risk);

    /**
     * @brief Gets the selected next deep sleep duration.
     * @return uint32_t Duration in seconds.
     */
    uint32_t sleepDurationSec() const;

    /**
     * @brief Updates RTC-retained time before entering deep sleep.
     * @param sleep_sec The sleep duration that will be passed to esp_deep_sleep().
     */
    void prepareForDeepSleep(uint32_t sleep_sec);

    /**
     * @brief Applies an alert mode command received from the network.
     * @param command The alert mode command payload.
     */
    void applyAlertModeCommand(const WedsAlertModeEnablePayload& command);


    /**
     * @brief Checks if alert mode is currently active.
     * @return true if active, false otherwise.
     */
    bool alertModeActive() const;

    /**
     * @brief Gets the unique node identifier.
     * @return uint32_t The node ID.
     */
    uint32_t nodeId() const;

private:
    uint32_t node_id_;
    uint32_t wake_start_ms_;

    /**
     * @brief Reads the battery level of the node.
     * @return float The battery level percentage (0.0 to 100.0).
     */
    float readBatteryLevel() const;

    void activateAlertMode(
        uint32_t source_node_id,
        uint32_t duration_sec,
        uint32_t sampling_interval_sec
    );
    void refreshAlertModeExpiry();
};
