#pragma once

#include <stdint.h>
#include <stddef.h>

static constexpr uint8_t WEDS_MAGIC = 0xA7;

static constexpr uint32_t WEDS_GATEWAY_ID = 0;

static constexpr size_t WEDS_MAX_PAYLOAD_SIZE = 64;
static constexpr size_t WEDS_HEADER_SIZE = 14;
static constexpr size_t WEDS_CRC_SIZE = 2;
static constexpr size_t WEDS_MAX_PACKET_SIZE =
    WEDS_HEADER_SIZE + WEDS_MAX_PAYLOAD_SIZE + WEDS_CRC_SIZE;

/**
 * @brief Represents the various supported message types in the WEDS protocol.
 */
enum WedsMessageType : uint8_t {
    WEDS_MSG_NODE_STATUS       = 0x01,
    WEDS_MSG_NODE_ALERT        = 0x02,
    WEDS_MSG_ACK               = 0x03,
    WEDS_MSG_ALERT_MODE_ENABLE = 0x10
};

/**
 * @brief Indicates whether a packet requires an acknowledgment.
 */
enum WedsAckRequired : uint8_t {
    WEDS_ACK_NOT_REQUIRED = 0,
    WEDS_ACK_REQUIRED     = 1
};

/**
 * @brief Discrete state representing normal or alert conditions for a node.
 */
enum WedsDetectionState : uint8_t {
    WEDS_DETECTION_NORMAL = 0,
    WEDS_DETECTION_ALERT  = 1
};

/**
 * @brief Status codes embedded in ACK payloads.
 */
enum WedsAckStatus : uint8_t {
    WEDS_ACK_STATUS_OK = 0,               /**< Acknowledgment successful, no errors. */
    WEDS_ACK_STATUS_INVALID_PACKET = 1,   /**< Packet is malformed or invalid. */
    WEDS_ACK_STATUS_UNSUPPORTED_MSG = 2,  /**< Message type is unsupported. */
    WEDS_ACK_STATUS_CRC_ERROR = 3         /**< CRC validation failed. */
};