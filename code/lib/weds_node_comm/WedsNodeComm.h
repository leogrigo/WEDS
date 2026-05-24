#pragma once

#include <Arduino.h>
#include "WedsProtocol.h"

/**
 * @class WedsNodeComm
 * @brief Handles LoRa communication for the sensor node, including status, alerts, and command polling.
 */
class WedsNodeComm {
public:
    /**
     * @brief Constructs a new WedsNodeComm object.
     */
    WedsNodeComm();

    /**
     * @brief Initializes the communication module and the LoRa radio.
     * @return true if initialization was successful, false otherwise.
     */
    bool begin();

    /**
     * @brief Handles incoming messages and maintains communication state. Must be called regularly.
     */
    void loop();

    /**
     * @brief Sends a status update payload to the gateway.
     * @param status The status payload to send.
     * @return true if the transmission was initiated successfully.
     */
    bool sendStatus(const WedsNodeStatusPayload& status);

    /**
     * @brief Sends an alert to the gateway, ensuring delivery through retries and ACK handling.
     * @param status The alert status payload.
     * @return true if the alert was successfully sent and acknowledged.
     */
    bool sendAlert(const WedsNodeStatusPayload& status);

    /**
     * @brief Polls for an incoming Alert Mode Enable command from the gateway.
     * @param out_command The structure to populate with the command payload.
     * @param timeout_ms The maximum time to wait for a command, in milliseconds.
     * @return true if a command was received and decoded successfully.
     */
    bool pollAlertModeEnable(
        WedsAlertModeEnablePayload& out_command,
        uint32_t timeout_ms
    );

    /**
     * @brief Puts the LoRa radio into a low-power sleep state.
     */
    void sleepRadio();

    /**
     * @brief Wakes the LoRa radio from sleep mode.
     */
    void wakeRadio();

    /**
     * @brief Gets the unique identifier for this node.
     * @return uint32_t The node ID.
     */
    uint32_t getNodeId() const;

    /**
     * @brief Gets the current sequence ID for outgoing messages.
     * @return uint16_t The sequence ID.
     */
    uint16_t getCurrentSequenceId() const;

private:
    uint32_t node_id_;
    uint16_t sequence_id_;
    bool initialized_;
    bool radio_sleeping_;
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
