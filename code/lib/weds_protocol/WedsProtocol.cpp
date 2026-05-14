#include "WedsProtocol.h"

#include <string.h>

#if defined(ARDUINO)
#include <Arduino.h>
#endif

// =========================
// Internal little-endian helpers
// =========================

// CRC16-CCITT-FALSE
// Polynomial: 0x1021
// Initial value: 0xFFFF
static uint16_t weds_crc16_ccitt(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;

        for (uint8_t bit = 0; bit < 8; ++bit) {
            if (crc & 0x8000) {
                crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
            } else {
                crc = static_cast<uint16_t>(crc << 1);
            }
        }
    }

    return crc;
}

static bool write_u8(uint8_t* buffer, size_t buffer_size, size_t& offset, uint8_t value) {
    if (offset + 1 > buffer_size) return false;
    buffer[offset++] = value;
    return true;
}

static bool write_u16_le(uint8_t* buffer, size_t buffer_size, size_t& offset, uint16_t value) {
    if (offset + 2 > buffer_size) return false;
    buffer[offset++] = static_cast<uint8_t>(value & 0xFF);
    buffer[offset++] = static_cast<uint8_t>((value >> 8) & 0xFF);
    return true;
}

static bool write_u32_le(uint8_t* buffer, size_t buffer_size, size_t& offset, uint32_t value) {
    if (offset + 4 > buffer_size) return false;
    buffer[offset++] = static_cast<uint8_t>(value & 0xFF);
    buffer[offset++] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buffer[offset++] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buffer[offset++] = static_cast<uint8_t>((value >> 24) & 0xFF);
    return true;
}

static bool write_float_le(uint8_t* buffer, size_t buffer_size, size_t& offset, float value) {
    static_assert(sizeof(float) == 4, "float must be 4 bytes");

    uint32_t raw;
    memcpy(&raw, &value, sizeof(float));

    return write_u32_le(buffer, buffer_size, offset, raw);
}

static bool read_u8(const uint8_t* buffer, size_t len, size_t& offset, uint8_t& out) {
    if (offset + 1 > len) return false;
    out = buffer[offset++];
    return true;
}

static bool read_u16_le(const uint8_t* buffer, size_t len, size_t& offset, uint16_t& out) {
    if (offset + 2 > len) return false;

    out = static_cast<uint16_t>(buffer[offset])
        | static_cast<uint16_t>(buffer[offset + 1] << 8);

    offset += 2;
    return true;
}

static bool read_u32_le(const uint8_t* buffer, size_t len, size_t& offset, uint32_t& out) {
    if (offset + 4 > len) return false;

    out = static_cast<uint32_t>(buffer[offset])
        | (static_cast<uint32_t>(buffer[offset + 1]) << 8)
        | (static_cast<uint32_t>(buffer[offset + 2]) << 16)
        | (static_cast<uint32_t>(buffer[offset + 3]) << 24);

    offset += 4;
    return true;
}

static bool read_float_le(const uint8_t* buffer, size_t len, size_t& offset, float& out) {
    uint32_t raw;

    if (!read_u32_le(buffer, len, offset, raw)) {
        return false;
    }

    memcpy(&out, &raw, sizeof(float));
    return true;
}

// =========================
// Node ID helper
// =========================

uint32_t weds_get_node_id_from_mac() {
#if defined(ARDUINO)
    uint64_t mac = ESP.getEfuseMac();
    return static_cast<uint32_t>(mac & 0xFFFFFFFF);
#else
    return 0;
#endif
}

// =========================
// Packet serialization
// =========================

bool weds_serialize_packet(
    const WedsPacket& packet,
    uint8_t* buffer,
    size_t buffer_size,
    size_t& out_len
) {
    out_len = 0;

    if (packet.header.payload_len > WEDS_MAX_PAYLOAD_SIZE) {
        return false;
    }

    const size_t total_without_crc = WEDS_HEADER_SIZE + packet.header.payload_len;
    const size_t total_with_crc = total_without_crc + WEDS_CRC_SIZE;

    if (buffer_size < total_with_crc) {
        return false;
    }

    size_t offset = 0;

    if (!write_u8(buffer, buffer_size, offset, packet.header.magic)) return false;
    if (!write_u8(buffer, buffer_size, offset, packet.header.msg_type)) return false;
    if (!write_u8(buffer, buffer_size, offset, packet.header.ack_required)) return false;
    if (!write_u32_le(buffer, buffer_size, offset, packet.header.src_node_id)) return false;
    if (!write_u32_le(buffer, buffer_size, offset, packet.header.dst_node_id)) return false;
    if (!write_u16_le(buffer, buffer_size, offset, packet.header.sequence_id)) return false;
    if (!write_u8(buffer, buffer_size, offset, packet.header.payload_len)) return false;

    if (offset != WEDS_HEADER_SIZE) {
        return false;
    }

    if (packet.header.payload_len > 0) {
        memcpy(buffer + offset, packet.payload, packet.header.payload_len);
        offset += packet.header.payload_len;
    }

    const uint16_t crc = weds_crc16_ccitt(buffer, total_without_crc);

    if (!write_u16_le(buffer, buffer_size, offset, crc)) return false;

    out_len = offset;
    return true;
}

bool weds_deserialize_packet(
    const uint8_t* buffer,
    size_t len,
    WedsPacket& out_packet
) {
    if (len < WEDS_HEADER_SIZE + WEDS_CRC_SIZE) {
        return false;
    }

    size_t offset = 0;

    if (!read_u8(buffer, len, offset, out_packet.header.magic)) return false;
    if (!read_u8(buffer, len, offset, out_packet.header.msg_type)) return false;
    if (!read_u8(buffer, len, offset, out_packet.header.ack_required)) return false;
    if (!read_u32_le(buffer, len, offset, out_packet.header.src_node_id)) return false;
    if (!read_u32_le(buffer, len, offset, out_packet.header.dst_node_id)) return false;
    if (!read_u16_le(buffer, len, offset, out_packet.header.sequence_id)) return false;
    if (!read_u8(buffer, len, offset, out_packet.header.payload_len)) return false;

    if (out_packet.header.magic != WEDS_MAGIC) {
        return false;
    }

    if (out_packet.header.payload_len > WEDS_MAX_PAYLOAD_SIZE) {
        return false;
    }

    const size_t expected_len =
        WEDS_HEADER_SIZE + out_packet.header.payload_len + WEDS_CRC_SIZE;

    if (len != expected_len) {
        return false;
    }

    if (out_packet.header.payload_len > 0) {
        memcpy(out_packet.payload, buffer + offset, out_packet.header.payload_len);
        offset += out_packet.header.payload_len;
    }

    uint16_t received_crc;
    if (!read_u16_le(buffer, len, offset, received_crc)) return false;

    const uint16_t computed_crc =
        weds_crc16_ccitt(buffer, WEDS_HEADER_SIZE + out_packet.header.payload_len);

    if (received_crc != computed_crc) {
        return false;
    }

    return true;
}

// =========================
// NODE_STATUS / NODE_ALERT payload
// =========================

bool weds_encode_node_status_payload(
    const WedsNodeStatusPayload& payload,
    uint8_t* buffer,
    size_t buffer_size,
    size_t& out_len
) {
    out_len = 0;
    size_t offset = 0;

    if (!write_u32_le(buffer, buffer_size, offset, payload.timestamp_s)) return false;

    if (!write_float_le(buffer, buffer_size, offset, payload.temperature)) return false;
    if (!write_float_le(buffer, buffer_size, offset, payload.humidity)) return false;
    if (!write_float_le(buffer, buffer_size, offset, payload.pressure)) return false;
    if (!write_float_le(buffer, buffer_size, offset, payload.gas_resistance)) return false;
    if (!write_float_le(buffer, buffer_size, offset, payload.battery_level)) return false;

    if (!write_u8(buffer, buffer_size, offset, payload.anomaly_state)) return false;
    if (!write_float_le(buffer, buffer_size, offset, payload.anomaly_score)) return false;

    if (!write_u8(buffer, buffer_size, offset, payload.risk_state)) return false;
    if (!write_float_le(buffer, buffer_size, offset, payload.risk_score)) return false;

    out_len = offset;
    return true;
}

bool weds_decode_node_status_payload(
    const uint8_t* buffer,
    size_t len,
    WedsNodeStatusPayload& out_payload
) {
    size_t offset = 0;

    if (!read_u32_le(buffer, len, offset, out_payload.timestamp_s)) return false;

    if (!read_float_le(buffer, len, offset, out_payload.temperature)) return false;
    if (!read_float_le(buffer, len, offset, out_payload.humidity)) return false;
    if (!read_float_le(buffer, len, offset, out_payload.pressure)) return false;
    if (!read_float_le(buffer, len, offset, out_payload.gas_resistance)) return false;
    if (!read_float_le(buffer, len, offset, out_payload.battery_level)) return false;

    if (!read_u8(buffer, len, offset, out_payload.anomaly_state)) return false;
    if (!read_float_le(buffer, len, offset, out_payload.anomaly_score)) return false;

    if (!read_u8(buffer, len, offset, out_payload.risk_state)) return false;
    if (!read_float_le(buffer, len, offset, out_payload.risk_score)) return false;

    return offset == len;
}

// =========================
// ACK payload
// =========================

bool weds_encode_ack_payload(
    const WedsAckPayload& payload,
    uint8_t* buffer,
    size_t buffer_size,
    size_t& out_len
) {
    out_len = 0;
    size_t offset = 0;

    if (!write_u16_le(buffer, buffer_size, offset, payload.acked_sequence_id)) return false;
    if (!write_u8(buffer, buffer_size, offset, payload.acked_msg_type)) return false;
    if (!write_u8(buffer, buffer_size, offset, payload.status_code)) return false;

    out_len = offset;
    return true;
}

bool weds_decode_ack_payload(
    const uint8_t* buffer,
    size_t len,
    WedsAckPayload& out_payload
) {
    size_t offset = 0;

    if (!read_u16_le(buffer, len, offset, out_payload.acked_sequence_id)) return false;
    if (!read_u8(buffer, len, offset, out_payload.acked_msg_type)) return false;
    if (!read_u8(buffer, len, offset, out_payload.status_code)) return false;

    return offset == len;
}

// =========================
// ALERT_MODE_ENABLE payload
// =========================

bool weds_encode_alert_mode_enable_payload(
    const WedsAlertModeEnablePayload& payload,
    uint8_t* buffer,
    size_t buffer_size,
    size_t& out_len
) {
    out_len = 0;
    size_t offset = 0;

    if (!write_u32_le(buffer, buffer_size, offset, payload.alert_source_node_id)) return false;
    if (!write_u16_le(buffer, buffer_size, offset, payload.duration_sec)) return false;
    if (!write_u16_le(buffer, buffer_size, offset, payload.sampling_interval_sec)) return false;

    out_len = offset;
    return true;
}

bool weds_decode_alert_mode_enable_payload(
    const uint8_t* buffer,
    size_t len,
    WedsAlertModeEnablePayload& out_payload
) {
    size_t offset = 0;

    if (!read_u32_le(buffer, len, offset, out_payload.alert_source_node_id)) return false;
    if (!read_u16_le(buffer, len, offset, out_payload.duration_sec)) return false;
    if (!read_u16_le(buffer, len, offset, out_payload.sampling_interval_sec)) return false;

    return offset == len;
}

// =========================
// Packet builders
// =========================

bool weds_build_node_status_packet(
    uint32_t src_node_id,
    uint16_t sequence_id,
    const WedsNodeStatusPayload& status,
    WedsPacket& out_packet
) {
    memset(&out_packet, 0, sizeof(WedsPacket));

    out_packet.header.magic = WEDS_MAGIC;
    out_packet.header.msg_type = WEDS_MSG_NODE_STATUS;
    out_packet.header.ack_required = WEDS_ACK_NOT_REQUIRED;
    out_packet.header.src_node_id = src_node_id;
    out_packet.header.dst_node_id = WEDS_GATEWAY_ID;
    out_packet.header.sequence_id = sequence_id;

    size_t payload_len = 0;
    if (!weds_encode_node_status_payload(
            status,
            out_packet.payload,
            WEDS_MAX_PAYLOAD_SIZE,
            payload_len
        )) {
        return false;
    }

    out_packet.header.payload_len = static_cast<uint8_t>(payload_len);
    return true;
}

bool weds_build_node_alert_packet(
    uint32_t src_node_id,
    uint16_t sequence_id,
    const WedsNodeStatusPayload& status,
    WedsPacket& out_packet
) {
    memset(&out_packet, 0, sizeof(WedsPacket));

    out_packet.header.magic = WEDS_MAGIC;
    out_packet.header.msg_type = WEDS_MSG_NODE_ALERT;
    out_packet.header.ack_required = WEDS_ACK_REQUIRED;
    out_packet.header.src_node_id = src_node_id;
    out_packet.header.dst_node_id = WEDS_GATEWAY_ID;
    out_packet.header.sequence_id = sequence_id;

    size_t payload_len = 0;
    if (!weds_encode_node_status_payload(
            status,
            out_packet.payload,
            WEDS_MAX_PAYLOAD_SIZE,
            payload_len
        )) {
        return false;
    }

    out_packet.header.payload_len = static_cast<uint8_t>(payload_len);
    return true;
}

bool weds_build_ack_packet(
    uint32_t src_node_id,
    uint32_t dst_node_id,
    uint16_t sequence_id,
    uint16_t acked_sequence_id,
    uint8_t acked_msg_type,
    uint8_t status_code,
    WedsPacket& out_packet
) {
    memset(&out_packet, 0, sizeof(WedsPacket));

    out_packet.header.magic = WEDS_MAGIC;
    out_packet.header.msg_type = WEDS_MSG_ACK;
    out_packet.header.ack_required = WEDS_ACK_NOT_REQUIRED;
    out_packet.header.src_node_id = src_node_id;
    out_packet.header.dst_node_id = dst_node_id;
    out_packet.header.sequence_id = sequence_id;

    WedsAckPayload ack_payload;
    ack_payload.acked_sequence_id = acked_sequence_id;
    ack_payload.acked_msg_type = acked_msg_type;
    ack_payload.status_code = status_code;

    size_t payload_len = 0;
    if (!weds_encode_ack_payload(
            ack_payload,
            out_packet.payload,
            WEDS_MAX_PAYLOAD_SIZE,
            payload_len
        )) {
        return false;
    }

    out_packet.header.payload_len = static_cast<uint8_t>(payload_len);
    return true;
}

bool weds_build_alert_mode_enable_packet(
    uint32_t dst_node_id,
    uint16_t sequence_id,
    const WedsAlertModeEnablePayload& command,
    WedsPacket& out_packet
) {
    memset(&out_packet, 0, sizeof(WedsPacket));

    out_packet.header.magic = WEDS_MAGIC;
    out_packet.header.msg_type = WEDS_MSG_ALERT_MODE_ENABLE;
    out_packet.header.ack_required = WEDS_ACK_REQUIRED;
    out_packet.header.src_node_id = WEDS_GATEWAY_ID;
    out_packet.header.dst_node_id = dst_node_id;
    out_packet.header.sequence_id = sequence_id;

    size_t payload_len = 0;
    if (!weds_encode_alert_mode_enable_payload(
            command,
            out_packet.payload,
            WEDS_MAX_PAYLOAD_SIZE,
            payload_len
        )) {
        return false;
    }

    out_packet.header.payload_len = static_cast<uint8_t>(payload_len);
    return true;
}
