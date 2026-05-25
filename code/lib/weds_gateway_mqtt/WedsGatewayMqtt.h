#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#include "WedsGatewayComm.h"
#include "WedsGatewayRegistry.h"

/**
 * @class WedsGatewayMqtt
 * @brief Bridges WEDS LoRa nodes to a local MQTT broker.
 */
class WedsGatewayMqtt {
public:
    WedsGatewayMqtt();

    /**
     * @brief Initializes MQTT client settings.
     *
     * @param host MQTT broker host.
     * @param port MQTT broker port.
     * @param registry Gateway registry pointer.
     * @param gateway_comm Gateway LoRa communication pointer.
     * @return true If configuration was accepted.
     */
    bool begin(
        const char* host,
        uint16_t port,
        WedsGatewayRegistry* registry,
        WedsGatewayComm* gateway_comm
    );

    /**
     * @brief Maintains MQTT connection and publishes pending node updates.
     */
    void loop();

    /**
     * @brief Handles an MQTT message delivered by PubSubClient.
     */
    void handleMessage(char* topic, uint8_t* payload, unsigned int length);

private:
    struct NodePublishState {
        uint32_t node_id;
        uint16_t last_sequence_id;
        uint8_t last_msg_type;
        bool telemetry_published;
    };

    WiFiClient wifi_client_;
    PubSubClient mqtt_;

    const char* host_;
    uint16_t port_;
    WedsGatewayRegistry* registry_;
    WedsGatewayComm* gateway_comm_;
    bool initialized_;
    uint32_t last_reconnect_attempt_ms_;
    uint32_t last_status_publish_ms_;
    char client_id_[48];
    NodePublishState node_states_[WEDS_MAX_NODES];
    WedsNodeRecord publish_records_[WEDS_MAX_NODES];

    bool ensureConnected();
    bool connectMqtt();
    void subscribeTopics();
    void publishGatewayStatus(bool online);
    void publishGatewayStatusIfDue();
    void publishKnownNodes();
    bool publishNodeTelemetry(const WedsNodeRecord& record);

    void handleCommand(const uint8_t* payload, unsigned int length);
    void handleEnableAlertModeCommand(
        const char* command_id,
        uint32_t node_id,
        JsonVariantConst params
    );
    void handleSetLocationCommand(
        const char* command_id,
        uint32_t node_id,
        JsonVariantConst params
    );
    void handleClearConfigCommand(const char* command_id);
    void publishCommandResponse(
        const char* command_id,
        const char* method,
        uint32_t node_id,
        bool success,
        const char* message,
        bool accepted = false,
        bool delivered = false,
        bool pending = false
    );

    NodePublishState* stateForNode(uint32_t node_id);
    bool credentialsConfigured() const;
};
