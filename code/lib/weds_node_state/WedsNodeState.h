#pragma once

#include <Arduino.h>

#include "WedsProtocol.h"
#include "WedsAnomalyDetector.h"
#include "WedsRiskScore.h"
#include "WedsSensorSample.h"

class WedsNodeState {
public:
    WedsNodeState();

    void begin(uint32_t node_id);

    WedsNodeStatusPayload buildPayload(
        const WedsSensorSample& sample,
        const WedsAnomalyResult& anomaly,
        const WedsRiskResult& risk
    ) const;

    void setSleepEnabled(bool enabled);
    bool sleepEnabled() const;

    void applyAlertModeCommand(const WedsAlertModeEnablePayload& command);
    void refreshAlertMode();
    bool alertModeActive() const;
    uint32_t sampleIntervalMs() const;

    uint32_t nodeId() const;

private:
    uint32_t node_id_;
    bool sleep_enabled_;
    bool alert_mode_active_;
    uint32_t alert_mode_until_ms_;
    uint32_t normal_sample_interval_ms_;
    uint32_t active_sample_interval_ms_;
    uint32_t alert_source_node_id_;

    float readBatteryLevel() const;
};
