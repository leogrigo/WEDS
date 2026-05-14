#pragma once

#include "WedsTypes.h"
#include <stdint.h>

// =========================
// Packet Header
// =========================
//
// Binary layout:
//
// magic          uint8_t   1 byte
// msg_type       uint8_t   1 byte
// ack_required   uint8_t   1 byte
// src_node_id    uint32_t  4 bytes
// dst_node_id    uint32_t  4 bytes
// sequence_id    uint16_t  2 bytes
// payload_len    uint8_t   1 byte
//
// Total header size = 14 bytes
//
// CRC16 is appended after payload.

struct WedsPacketHeader {
    uint8_t magic;
    uint8_t msg_type;
    uint8_t ack_required;
    uint32_t src_node_id;
    uint32_t dst_node_id;
    uint16_t sequence_id;
    uint8_t payload_len;
};

// =========================
// Generic packet
// =========================

struct WedsPacket {
    WedsPacketHeader header;
    uint8_t payload[WEDS_MAX_PAYLOAD_SIZE];
};

// =========================
// NODE_STATUS / NODE_ALERT payload
// =========================
//
// Same payload for NODE_STATUS and NODE_ALERT.
// Difference is in msg_type and ack_required.

struct WedsNodeStatusPayload {
    uint32_t timestamp_s;

    float temperature;
    float humidity;
    float pressure;
    float gas_resistance;
    float battery_level;

    uint8_t anomaly_state;
    float anomaly_score;

    uint8_t risk_state;
    float risk_score;
};

// =========================
// ACK payload
// =========================

struct WedsAckPayload {
    uint16_t acked_sequence_id;
    uint8_t acked_msg_type;
    uint8_t status_code;
};

// =========================
// ALERT_MODE_ENABLE payload
// =========================

struct WedsAlertModeEnablePayload {
    uint32_t alert_source_node_id;
    uint16_t duration_sec;
    uint16_t sampling_interval_sec;
};