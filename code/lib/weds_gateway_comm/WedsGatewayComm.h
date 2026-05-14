#pragma once

#include <Arduino.h>
#include "WedsProtocol.h"
#include "WedsGatewayRegistry.h"

class WedsGatewayComm {
public:
    WedsGatewayComm();

    bool begin(WedsGatewayRegistry* registry); // Initializes the communication module with a reference to the gateway registry
    void loop(); // Should be called regularly to handle incoming messages and maintain communication
    bool poll(); // Polls for incoming messages and processes them

private:
    WedsGatewayRegistry* registry_;
    bool initialized_;
    uint16_t gateway_sequence_id_;

    bool initRadio();

    void handleReceivedPacket(uint8_t* buffer, size_t len);
    void handleNodeStatusPacket(const WedsPacket& packet);

    bool sendAck(
        uint32_t dst_node_id,
        uint16_t acked_sequence_id,
        uint8_t acked_msg_type,
        uint8_t status_code
    );

    bool sendPacket(const WedsPacket& packet);

    bool waitForAck(
        uint32_t expected_src_node_id,
        uint16_t expected_acked_sequence_id,
        uint8_t expected_acked_msg_type,
        uint32_t timeout_ms
    );

    bool isExpectedAck(
        const WedsPacket& packet,
        uint32_t expected_src_node_id,
        uint16_t expected_acked_sequence_id,
        uint8_t expected_acked_msg_type
    );

    bool sendAlertModeEnableReliable(
        uint32_t dst_node_id,
        const WedsAlertModeEnablePayload& command
    );

    void deliverPendingCommandIfAny(uint32_t node_id);

    void printBufferHex(const uint8_t* buffer, size_t len);
    void printNodeStatus(
        const WedsPacket& packet,
        const WedsNodeStatusPayload& status
    );
};
