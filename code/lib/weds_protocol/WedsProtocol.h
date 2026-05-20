#pragma once

#include "WedsPacket.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @brief Derives a unique node ID from the hardware MAC address.
 * @return The generated 32-bit node ID.
 */
uint32_t weds_get_node_id_from_mac();

/**
 * @brief Serializes a WedsPacket into a byte buffer for transmission.
 * @param packet The packet to serialize.
 * @param buffer The destination buffer for the serialized bytes.
 * @param buffer_size The maximum size of the destination buffer.
 * @param out_len On success, the total number of bytes written.
 * @return True if serialization was successful, false otherwise.
 */
bool weds_serialize_packet(
    const WedsPacket& packet,
    uint8_t* buffer,
    size_t buffer_size,
    size_t& out_len
);

/**
 * @brief Deserializes a byte buffer into a WedsPacket structure.
 * @param buffer The source buffer containing the received bytes.
 * @param len The length of the source buffer.
 * @param out_packet On success, the populated packet.
 * @return True if deserialization was successful and CRC matched, false otherwise.
 */
bool weds_deserialize_packet(
    const uint8_t* buffer,
    size_t len,
    WedsPacket& out_packet
);

/**
 * @brief Encodes a NODE_STATUS payload into a byte buffer.
 * @param payload The status payload to encode.
 * @param buffer The destination buffer for the encoded bytes.
 * @param buffer_size The maximum size of the destination buffer.
 * @param out_len On success, the number of bytes written.
 * @return True if encoding was successful, false otherwise.
 */
bool weds_encode_node_status_payload(
    const WedsNodeStatusPayload& payload,
    uint8_t* buffer,
    size_t buffer_size,
    size_t& out_len
);

/**
 * @brief Decodes a byte buffer into a NODE_STATUS payload structure.
 * @param buffer The source buffer.
 * @param len The length of the source buffer.
 * @param out_payload On success, the decoded payload.
 * @return True if decoding was successful, false otherwise.
 */
bool weds_decode_node_status_payload(
    const uint8_t* buffer,
    size_t len,
    WedsNodeStatusPayload& out_payload
);

/**
 * @brief Encodes an ACK payload into a byte buffer.
 * @param payload The ACK payload to encode.
 * @param buffer The destination buffer.
 * @param buffer_size The maximum size of the destination buffer.
 * @param out_len On success, the number of bytes written.
 * @return True if encoding was successful, false otherwise.
 */
bool weds_encode_ack_payload(
    const WedsAckPayload& payload,
    uint8_t* buffer,
    size_t buffer_size,
    size_t& out_len
);

/**
 * @brief Decodes a byte buffer into an ACK payload structure.
 * @param buffer The source buffer.
 * @param len The length of the source buffer.
 * @param out_payload On success, the decoded payload.
 * @return True if decoding was successful, false otherwise.
 */
bool weds_decode_ack_payload(
    const uint8_t* buffer,
    size_t len,
    WedsAckPayload& out_payload
);

/**
 * @brief Encodes an ALERT_MODE_ENABLE payload into a byte buffer.
 * @param payload The alert enable command payload.
 * @param buffer The destination buffer.
 * @param buffer_size The maximum size of the destination buffer.
 * @param out_len On success, the number of bytes written.
 * @return True if encoding was successful, false otherwise.
 */
bool weds_encode_alert_mode_enable_payload(
    const WedsAlertModeEnablePayload& payload,
    uint8_t* buffer,
    size_t buffer_size,
    size_t& out_len
);

/**
 * @brief Decodes a byte buffer into an ALERT_MODE_ENABLE payload.
 * @param buffer The source buffer.
 * @param len The length of the source buffer.
 * @param out_payload On success, the decoded payload.
 * @return True if decoding was successful, false otherwise.
 */
bool weds_decode_alert_mode_enable_payload(
    const uint8_t* buffer,
    size_t len,
    WedsAlertModeEnablePayload& out_payload
);

/**
 * @brief Builds a complete NODE_STATUS packet.
 * @param src_node_id The source node identifier.
 * @param sequence_id The packet sequence identifier.
 * @param status The status payload to include.
 * @param out_packet On success, the constructed packet.
 * @return True if construction was successful, false otherwise.
 */
bool weds_build_node_status_packet(
    uint32_t src_node_id,
    uint16_t sequence_id,
    const WedsNodeStatusPayload& status,
    WedsPacket& out_packet
);

/**
 * @brief Builds a complete NODE_ALERT packet.
 * @param src_node_id The source node identifier.
 * @param sequence_id The packet sequence identifier.
 * @param status The status payload to include.
 * @param out_packet On success, the constructed packet.
 * @return True if construction was successful, false otherwise.
 */
bool weds_build_node_alert_packet(
    uint32_t src_node_id,
    uint16_t sequence_id,
    const WedsNodeStatusPayload& status,
    WedsPacket& out_packet
);

/**
 * @brief Builds a complete ACK packet.
 * @param src_node_id The ID of the node sending the ACK.
 * @param dst_node_id The ID of the node intended to receive the ACK.
 * @param sequence_id The sequence ID for this ACK packet.
 * @param acked_sequence_id The sequence ID of the packet being acknowledged.
 * @param acked_msg_type The message type of the packet being acknowledged.
 * @param status_code The status code of the acknowledgment.
 * @param out_packet On success, the constructed packet.
 * @return True if construction was successful, false otherwise.
 */
bool weds_build_ack_packet(
    uint32_t src_node_id,
    uint32_t dst_node_id,
    uint16_t sequence_id,
    uint16_t acked_sequence_id,
    uint8_t acked_msg_type,
    uint8_t status_code,
    WedsPacket& out_packet
);

/**
 * @brief Builds a complete ALERT_MODE_ENABLE command packet.
 * @param dst_node_id The destination node identifier.
 * @param sequence_id The sequence ID for this command packet.
 * @param command The alert enable configuration payload.
 * @param out_packet On success, the constructed packet.
 * @return True if construction was successful, false otherwise.
 */
bool weds_build_alert_mode_enable_packet(
    uint32_t dst_node_id,
    uint16_t sequence_id,
    const WedsAlertModeEnablePayload& command,
    WedsPacket& out_packet
);