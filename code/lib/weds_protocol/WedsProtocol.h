#pragma once

#include "WedsPacket.h"
#include <stdint.h>
#include <stddef.h>

// =========================
// Node ID helper
// =========================

uint32_t weds_get_node_id_from_mac();

// =========================
// Packet serialization
// =========================

bool weds_serialize_packet(
    const WedsPacket& packet,
    uint8_t* buffer,
    size_t buffer_size,
    size_t& out_len
);

bool weds_deserialize_packet(
    const uint8_t* buffer,
    size_t len,
    WedsPacket& out_packet
);

// =========================
// Payload serialization
// =========================

bool weds_encode_node_status_payload(
    const WedsNodeStatusPayload& payload,
    uint8_t* buffer,
    size_t buffer_size,
    size_t& out_len
);

bool weds_decode_node_status_payload(
    const uint8_t* buffer,
    size_t len,
    WedsNodeStatusPayload& out_payload
);

bool weds_encode_ack_payload(
    const WedsAckPayload& payload,
    uint8_t* buffer,
    size_t buffer_size,
    size_t& out_len
);

bool weds_decode_ack_payload(
    const uint8_t* buffer,
    size_t len,
    WedsAckPayload& out_payload
);

bool weds_encode_alert_mode_enable_payload(
    const WedsAlertModeEnablePayload& payload,
    uint8_t* buffer,
    size_t buffer_size,
    size_t& out_len
);

bool weds_decode_alert_mode_enable_payload(
    const uint8_t* buffer,
    size_t len,
    WedsAlertModeEnablePayload& out_payload
);

// =========================
// Packet builders
// =========================

bool weds_build_node_status_packet(
    uint32_t src_node_id,
    uint16_t sequence_id,
    const WedsNodeStatusPayload& status,
    WedsPacket& out_packet
);

bool weds_build_node_alert_packet(
    uint32_t src_node_id,
    uint16_t sequence_id,
    const WedsNodeStatusPayload& status,
    WedsPacket& out_packet
);

bool weds_build_ack_packet(
    uint32_t src_node_id,
    uint32_t dst_node_id,
    uint16_t sequence_id,
    uint16_t acked_sequence_id,
    uint8_t acked_msg_type,
    uint8_t status_code,
    WedsPacket& out_packet
);

bool weds_build_alert_mode_enable_packet(
    uint32_t dst_node_id,
    uint16_t sequence_id,
    const WedsAlertModeEnablePayload& command,
    WedsPacket& out_packet
);