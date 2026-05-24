#pragma once

#include "WedsTypes.h"
#include <stdint.h>

/**
 * @brief Represents the header of a WEDS packet.
 * 
 * Total header size is 14 bytes. CRC16 is appended after the payload.
 */
struct WedsPacketHeader {
    uint8_t magic;         /**< Magic byte, expected to be WEDS_MAGIC. */
    uint8_t msg_type;      /**< Type of message being sent. */
    uint8_t ack_required;  /**< Indicates if an ACK is required (1) or not (0). */
    uint32_t src_node_id;  /**< Source node identifier. */
    uint32_t dst_node_id;  /**< Destination node identifier. */
    uint16_t sequence_id;  /**< Packet sequence identifier. */
    uint8_t payload_len;   /**< Length of the payload in bytes. */
};

/**
 * @brief A generic WEDS packet including header and max-sized payload.
 */
struct WedsPacket {
    WedsPacketHeader header;                     /**< The packet header. */
    uint8_t payload[WEDS_MAX_PAYLOAD_SIZE];      /**< The packet payload buffer. */
};

/**
 * @brief Payload for NODE_STATUS and NODE_ALERT messages.
 * 
 * These two message types share the same payload structure, differing only
 * in the msg_type and ack_required fields of the header.
 */
struct WedsNodeStatusPayload {
    uint32_t timestamp_s;    /**< Unix timestamp in seconds. */
    float temperature;       /**< Temperature in degrees Celsius. */
    float humidity;          /**< Relative humidity percentage. */
    float pressure;          /**< Atmospheric pressure. */
    float gas_resistance;    /**< Measured gas resistance. */
    float battery_level;     /**< Battery voltage or percentage. */
    uint8_t anomaly_state;   /**< Current anomaly state. */
    float anomaly_score;     /**< Computed anomaly score. */
    uint8_t risk_state;      /**< Current fire risk state. */
    float risk_score;        /**< Computed fire risk score. */
};

/**
 * @brief Payload for acknowledgment messages.
 */
struct WedsAckPayload {
    uint16_t acked_sequence_id;  /**< The sequence ID of the packet being acknowledged. */
    uint8_t acked_msg_type;      /**< The message type of the packet being acknowledged. */
    uint8_t status_code;         /**< Status code of the acknowledgment (see WedsAckStatus). */
};

/**
 * @brief Payload for command enabling an alert mode on a node.
 */
struct WedsAlertModeEnablePayload {
    uint32_t alert_source_node_id; /**< The ID of the node that triggered the alert. */
    uint16_t duration_sec;         /**< Duration to stay in alert mode, in seconds. */
    uint16_t sampling_interval_sec;/**< Sensor sampling interval in alert mode, in seconds. */
};