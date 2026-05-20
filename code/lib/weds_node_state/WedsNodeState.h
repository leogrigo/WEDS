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
     * @brief Enables or disables the node's ability to sleep.
     * @param enabled True to enable sleep, false otherwise.
     */
    void setSleepEnabled(bool enabled);

    /**
     * @brief Checks whether sleep mode is enabled.
     * @return true if sleep is enabled, false otherwise.
     */
    bool sleepEnabled() const;

    /**
     * @brief Applies an alert mode command received from the network.
     * @param command The alert mode command payload.
     */
    void applyAlertModeCommand(const WedsAlertModeEnablePayload& command);

    /**
     * @brief Updates the alert mode state, disabling it if the duration has expired.
     */
    void refreshAlertMode();

    /**
     * @brief Checks if alert mode is currently active.
     * @return true if active, false otherwise.
     */
    bool alertModeActive() const;

    /**
     * @brief Gets the current sampling interval in milliseconds.
     * @return uint32_t The sampling interval.
     */
    uint32_t sampleIntervalMs() const;

    /**
     * @brief Gets the unique node identifier.
     * @return uint32_t The node ID.
     */
    uint32_t nodeId() const;

private:
    uint32_t node_id_;
    bool sleep_enabled_;
    bool alert_mode_active_;
    uint32_t alert_mode_until_ms_;
    uint32_t normal_sample_interval_ms_;
    uint32_t active_sample_interval_ms_;
    uint32_t alert_source_node_id_;

    /**
     * @brief Reads the battery level of the node.
     * @return float The battery level percentage (0.0 to 100.0).
     */
    float readBatteryLevel() const;
};
