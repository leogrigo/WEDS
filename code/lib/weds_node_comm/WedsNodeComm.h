#pragma once

#include <Arduino.h>
#include "WedsProtocol.h"

class WedsNodeComm {
public:
    WedsNodeComm();

    bool begin();   // Initializes the communication module
    void loop();    // Should be called regularly to handle incoming messages and maintain communication

    bool sendStatus(const WedsNodeStatusPayload& status);   // Sends a status update to the gateway
    bool sendAlert(const WedsNodeStatusPayload& status);    // Sends an alert to the gateway, with retry and ACK handling

    bool pollAlertModeEnable(
        WedsAlertModeEnablePayload& out_command,
        uint32_t timeout_ms
    );  // Polls for an incoming Alert Mode Enable command from the gateway, with a specified timeout

    uint32_t getNodeId() const; // Returns the unique node ID of this device
    uint16_t getCurrentSequenceId() const;  // Returns the current sequence ID that will be used for the next outgoing message (before incrementing)

private:
    uint32_t node_id_;
    uint16_t sequence_id_;
    bool initialized_;
    bool has_last_gateway_command_;
    uint16_t last_gateway_command_sequence_id_;
    uint8_t last_gateway_command_msg_type_;

    bool initRadio();
    bool sendPacket(const WedsPacket& packet);

    bool waitForAck(
        uint16_t expected_acked_sequence_id,
        uint8_t expected_acked_msg_type,
        uint32_t timeout_ms
    );

    bool isExpectedAck(
        const WedsPacket& packet,
        uint16_t expected_acked_sequence_id,
        uint8_t expected_acked_msg_type
    );

    bool sendAck(
        uint32_t dst_node_id,
        uint16_t acked_sequence_id,
        uint8_t acked_msg_type,
        uint8_t status_code
    );

    bool isDuplicateGatewayCommand(const WedsPacket& packet) const;
    void markGatewayCommandProcessed(const WedsPacket& packet);

    void printBufferHex(const uint8_t* buffer, size_t len);
};
