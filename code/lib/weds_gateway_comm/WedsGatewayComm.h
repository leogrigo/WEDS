#pragma once

#include <Arduino.h>
#include "WedsProtocol.h"
#include "WedsGatewayRegistry.h"

/**
 * @class WedsGatewayComm
 * @brief Manages the LoRa radio communication for the gateway.
 */
class WedsGatewayComm {
public:
    /**
     * @brief Constructor for WedsGatewayComm.
     */
    WedsGatewayComm();

    /**
     * @brief Initializes the communication module.
     * 
     * @param registry Pointer to the gateway registry.
     * @return true If initialized successfully.
     * @return false If initialization fails.
     */
    bool begin(WedsGatewayRegistry* registry);

    /**
     * @brief Should be called regularly to handle incoming messages and maintain communication.
     */
    void loop();

    /**
     * @brief Polls for incoming messages and processes them.
     * 
     * @return true If a message was received and processed.
     * @return false If no message was available.
     */
    bool poll();

private:
    /** @brief Pointer to the gateway registry */
    WedsGatewayRegistry* registry_;
    
    /** @brief Indicates if the communication module has been initialized */
    bool initialized_;
    
    /** @brief Sequence ID for gateway messages */
    uint16_t gateway_sequence_id_;

    /**
     * @brief Initializes the LoRa radio hardware.
     * 
     * @return true If radio initialized successfully.
     * @return false If radio initialization fails.
     */
    bool initRadio();

    /**
     * @brief Handles a newly received packet from the radio.
     * 
     * @param buffer Pointer to the raw packet buffer.
     * @param len Length of the received packet.
     */
    void handleReceivedPacket(uint8_t* buffer, size_t len);
    
    /**
     * @brief Handles a parsed NODE_STATUS packet.
     * 
     * @param packet The parsed packet structure.
     */
    void handleNodeStatusPacket(const WedsPacket& packet);

    /**
     * @brief Sends an acknowledgment for a received reliable packet.
     * 
     * @param dst_node_id Destination node ID.
     * @param acked_sequence_id Sequence ID of the packet being acknowledged.
     * @param acked_msg_type Message type of the packet being acknowledged.
     * @param status_code Status code of the acknowledgment.
     * @return true If successfully sent.
     * @return false If sending failed.
     */
    bool sendAck(
        uint32_t dst_node_id,
        uint16_t acked_sequence_id,
        uint8_t acked_msg_type,
        uint8_t status_code
    );

    /**
     * @brief Sends a packet over the radio.
     * 
     * @param packet The packet structure to send.
     * @return true If successfully sent.
     * @return false If sending failed.
     */
    bool sendPacket(const WedsPacket& packet);

    /**
     * @brief Waits for an acknowledgment from a specific node.
     * 
     * @param expected_src_node_id Expected source node ID.
     * @param expected_acked_sequence_id Expected sequence ID being acknowledged.
     * @param expected_acked_msg_type Expected message type being acknowledged.
     * @param timeout_ms Timeout in milliseconds.
     * @return true If acknowledgment was received within the timeout.
     * @return false If timeout occurred.
     */
    bool waitForAck(
        uint32_t expected_src_node_id,
        uint16_t expected_acked_sequence_id,
        uint8_t expected_acked_msg_type,
        uint32_t timeout_ms
    );

    /**
     * @brief Checks if a packet is the expected acknowledgment.
     * 
     * @param packet The received packet.
     * @param expected_src_node_id Expected source node ID.
     * @param expected_acked_sequence_id Expected sequence ID.
     * @param expected_acked_msg_type Expected message type.
     * @return true If it matches the expected acknowledgment.
     * @return false Otherwise.
     */
    bool isExpectedAck(
        const WedsPacket& packet,
        uint32_t expected_src_node_id,
        uint16_t expected_acked_sequence_id,
        uint8_t expected_acked_msg_type
    );

    /**
     * @brief Reliably sends an ALERT_MODE_ENABLE command to a node.
     * 
     * @param dst_node_id Destination node ID.
     * @param command The command payload.
     * @return true If successfully sent and acknowledged.
     * @return false If failed or timed out.
     */
    bool sendAlertModeEnableReliable(
        uint32_t dst_node_id,
        const WedsAlertModeEnablePayload& command
    );

    /**
     * @brief Delivers a pending command to a node if one exists.
     * 
     * @param node_id The node ID to check for pending commands.
     */
    void deliverPendingCommandIfAny(uint32_t node_id);

    /**
     * @brief Helper to print a buffer in hexadecimal format.
     * 
     * @param buffer The buffer to print.
     * @param len The length of the buffer.
     */
    void printBufferHex(const uint8_t* buffer, size_t len);
    
    /**
     * @brief Helper to print parsed node status information.
     * 
     * @param packet The packet containing the status.
     * @param status The parsed status payload.
     */
    void printNodeStatus(
        const WedsPacket& packet,
        const WedsNodeStatusPayload& status
    );
};
