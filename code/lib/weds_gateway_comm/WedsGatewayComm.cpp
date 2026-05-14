#include "WedsGatewayComm.h"

#include <heltec_unofficial.h>
#include "WedsGatewayConfig.h"

namespace {

const char* messageTypeName(uint8_t msg_type) {
    switch (msg_type) {
        case WEDS_MSG_NODE_STATUS:
            return "NODE_STATUS";
        case WEDS_MSG_NODE_ALERT:
            return "NODE_ALERT";
        case WEDS_MSG_ACK:
            return "ACK";
        case WEDS_MSG_ALERT_MODE_ENABLE:
            return "ALERT_MODE_ENABLE";
        default:
            return "UNKNOWN";
    }
}

const char* detectionStateName(uint8_t state) {
    return state == WEDS_DETECTION_ALERT ? "ALERT" : "NORMAL";
}

}  // namespace

WedsGatewayComm::WedsGatewayComm()
    : registry_(nullptr),
      initialized_(false),
      gateway_sequence_id_(1) {}

bool WedsGatewayComm::begin(WedsGatewayRegistry* registry) {
    Serial.println("[GATEWAY_COMM] begin()");

    if (registry == nullptr) {
        Serial.println("[GATEWAY_COMM] begin failed: registry is null");
        initialized_ = false;
        return false;
    }

    registry_ = registry;

    heltec_setup();

    if (!initRadio()) {
        Serial.println("[GATEWAY_COMM] Radio init failed");
        initialized_ = false;
        return false;
    }

    initialized_ = true;
    Serial.println("[GATEWAY_COMM] Ready");

    return true;
}

void WedsGatewayComm::loop() {
    heltec_loop();
}

bool WedsGatewayComm::initRadio() {
    Serial.println("[GATEWAY_COMM] Initializing LoRa radio...");

    int state = radio.begin(
        WEDS_LORA_FREQUENCY_MHZ,
        WEDS_LORA_BANDWIDTH_KHZ,
        WEDS_LORA_SPREADING_FACTOR,
        WEDS_LORA_CODING_RATE,
        WEDS_LORA_SYNC_WORD,
        WEDS_LORA_OUTPUT_POWER_DBM,
        WEDS_LORA_PREAMBLE_LENGTH
    );

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("[GATEWAY_COMM] LoRa init OK");
        return true;
    }

    Serial.print("[GATEWAY_COMM] LoRa init failed, code=");
    Serial.println(state);
    return false;
}

bool WedsGatewayComm::poll() {
    if (!initialized_) {
        return false;
    }

    uint8_t rx_buffer[WEDS_MAX_PACKET_SIZE];

    int state = radio.receive(rx_buffer, sizeof(rx_buffer));

    if (state == RADIOLIB_ERR_NONE) {
        size_t len = radio.getPacketLength();
        handleReceivedPacket(rx_buffer, len);
        return true;
    }

    if (state == RADIOLIB_ERR_RX_TIMEOUT) {
        return false;
    }

    Serial.print("[GATEWAY_COMM] RX failed, code=");
    Serial.println(state);
    return false;
}

void WedsGatewayComm::handleReceivedPacket(uint8_t* buffer, size_t len) {
    Serial.println();

    Serial.print("[GATEWAY_COMM] Packet received len=");
    Serial.println(len);

    // Serial.print("[GATEWAY_COMM] HEX: ");
    // printBufferHex(buffer, len);

    WedsPacket packet;

    bool ok = weds_deserialize_packet(
        buffer,
        len,
        packet
    );

    if (!ok) {
        Serial.println("[GATEWAY_COMM] Invalid WEDS packet");
        return;
    }

    Serial.println("[GATEWAY_COMM] WEDS packet valid");

    if (packet.header.dst_node_id != WEDS_GATEWAY_ID) {
        Serial.print("[GATEWAY_COMM] Packet ignored, dst_node_id=");
        Serial.println(packet.header.dst_node_id);
        return;
    }

    if (
        packet.header.msg_type == WEDS_MSG_NODE_STATUS ||
        packet.header.msg_type == WEDS_MSG_NODE_ALERT
    ) {
        handleNodeStatusPacket(packet);
        return;
    }

    Serial.print("[GATEWAY_COMM] Unsupported msg_type=");
    Serial.println(messageTypeName(packet.header.msg_type));
}

void WedsGatewayComm::handleNodeStatusPacket(const WedsPacket& packet) {
    WedsNodeStatusPayload status;

    bool ok = weds_decode_node_status_payload(
        packet.payload,
        packet.header.payload_len,
        status
    );

    if (!ok) {
        Serial.println("[GATEWAY_COMM] Failed to decode NODE_STATUS/NODE_ALERT payload");

        if (packet.header.ack_required == WEDS_ACK_REQUIRED) {
            sendAck(
                packet.header.src_node_id,
                packet.header.sequence_id,
                packet.header.msg_type,
                WEDS_ACK_STATUS_INVALID_PACKET
            );
        }

        return;
    }

    const bool duplicate =
        packet.header.ack_required == WEDS_ACK_REQUIRED &&
        registry_->isDuplicateReliablePacket(
            packet.header.src_node_id,
            packet.header.sequence_id,
            packet.header.msg_type
        );

    if (duplicate) {
        Serial.println("[GATEWAY_COMM] Duplicate reliable packet detected");

        Serial.print("[GATEWAY_COMM] node_id=");
        Serial.print(packet.header.src_node_id);

        Serial.print(" seq=");
        Serial.print(packet.header.sequence_id);

        Serial.print(" msg_type=");
        Serial.println(messageTypeName(packet.header.msg_type));

        Serial.println("[GATEWAY_COMM] Duplicate will not be processed again");

        if (packet.header.ack_required == WEDS_ACK_REQUIRED) {
            Serial.println("[GATEWAY_COMM] Re-sending ACK for duplicate packet");

            sendAck(
                packet.header.src_node_id,
                packet.header.sequence_id,
                packet.header.msg_type,
                WEDS_ACK_STATUS_OK
            );
        }

        return;
    }

    const bool updated = registry_->updateNodeStatus(
        packet.header.src_node_id,
        packet.header.sequence_id,
        packet.header.msg_type,
        status
    );

    if (!updated) {
        Serial.println("[GATEWAY_COMM] Registry update failed");
    }

    printNodeStatus(packet, status);

    if (packet.header.ack_required == WEDS_ACK_REQUIRED) {
        Serial.println("[GATEWAY_COMM] Packet requires ACK, sending ACK...");

        sendAck(
            packet.header.src_node_id,
            packet.header.sequence_id,
            packet.header.msg_type,
            WEDS_ACK_STATUS_OK
        );
    }

    if (packet.header.msg_type == WEDS_MSG_NODE_ALERT) {
        registry_->createAlertCommandsForNeighbors(
            packet.header.src_node_id,
            status
        );
    }

    deliverPendingCommandIfAny(packet.header.src_node_id);
}

bool WedsGatewayComm::sendAck(
    uint32_t dst_node_id,
    uint16_t acked_sequence_id,
    uint8_t acked_msg_type,
    uint8_t status_code
) {
    WedsPacket ack_packet;

    const uint16_t ack_sequence_id = gateway_sequence_id_++;

    bool ok = weds_build_ack_packet(
        WEDS_GATEWAY_ID,
        dst_node_id,
        ack_sequence_id,
        acked_sequence_id,
        acked_msg_type,
        status_code,
        ack_packet
    );

    if (!ok) {
        Serial.println("[GATEWAY_COMM] Failed to build ACK packet");
        return false;
    }

    Serial.print("[GATEWAY_COMM] Sending ACK to node=");
    Serial.print(dst_node_id);

    Serial.print(" acked_seq=");
    Serial.print(acked_sequence_id);

    Serial.print(" acked_msg_type=");
    Serial.println(messageTypeName(acked_msg_type));

    return sendPacket(ack_packet);
}

bool WedsGatewayComm::sendPacket(const WedsPacket& packet) {
    uint8_t buffer[WEDS_MAX_PACKET_SIZE];
    size_t encoded_len = 0;

    bool ok = weds_serialize_packet(
        packet,
        buffer,
        sizeof(buffer),
        encoded_len
    );

    if (!ok) {
        Serial.println("[GATEWAY_COMM] Failed to serialize TX packet");
        return false;
    }

    Serial.print("[GATEWAY_COMM] TX msg_type=");
    Serial.print(messageTypeName(packet.header.msg_type));

    Serial.print(" seq=");
    Serial.print(packet.header.sequence_id);

    Serial.print(" dst=");
    Serial.print(packet.header.dst_node_id);

    Serial.print(" len=");
    Serial.println(encoded_len);

    // Serial.print("[GATEWAY_COMM] TX HEX: ");
    // printBufferHex(buffer, encoded_len);

    int state = radio.transmit(buffer, encoded_len);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("[GATEWAY_COMM] TX OK");
        return true;
    }

    Serial.print("[GATEWAY_COMM] TX failed, code=");
    Serial.println(state);
    return false;
}

void WedsGatewayComm::printBufferHex(const uint8_t* buffer, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (buffer[i] < 0x10) {
            Serial.print("0");
        }

        Serial.print(buffer[i], HEX);
        Serial.print(" ");
    }

    Serial.println();
}

void WedsGatewayComm::printNodeStatus(
    const WedsPacket& packet,
    const WedsNodeStatusPayload& status
) {
    Serial.println();

    Serial.printf(
        "[GATEWAY_COMM] %s node=%lu seq=%u ack=%u time=%lu\n",
        messageTypeName(packet.header.msg_type),
        static_cast<unsigned long>(packet.header.src_node_id),
        packet.header.sequence_id,
        packet.header.ack_required,
        static_cast<unsigned long>(status.timestamp_s)
    );
    Serial.printf(
        "[GATEWAY_COMM] sample temp=%.2f C hum=%.2f %% pressure=%.2f Pa gas=%.0f battery=%.1f %%\n",
        status.temperature,
        status.humidity,
        status.pressure,
        status.gas_resistance,
        status.battery_level
    );
    Serial.printf(
        "[GATEWAY_COMM] anomaly=%s score=%.2f risk=%s score=%.2f\n",
        detectionStateName(status.anomaly_state),
        status.anomaly_score,
        detectionStateName(status.risk_state),
        status.risk_score
    );
}

bool WedsGatewayComm::waitForAck(
    uint32_t expected_src_node_id,
    uint16_t expected_acked_sequence_id,
    uint8_t expected_acked_msg_type,
    uint32_t timeout_ms
) {
    const uint32_t start_ms = millis();

    while (millis() - start_ms < timeout_ms) {
        loop();

        uint8_t rx_buffer[WEDS_MAX_PACKET_SIZE];

        int state = radio.receive(rx_buffer, sizeof(rx_buffer));

        if (state == RADIOLIB_ERR_NONE) {
            const size_t len = radio.getPacketLength();

            Serial.print("[GATEWAY_COMM] RX while waiting ACK len=");
            Serial.println(len);

            WedsPacket packet;

            bool ok = weds_deserialize_packet(
                rx_buffer,
                len,
                packet
            );

            if (!ok) {
                Serial.println("[GATEWAY_COMM] Invalid packet while waiting ACK");
                continue;
            }

            if (isExpectedAck(
                    packet,
                    expected_src_node_id,
                    expected_acked_sequence_id,
                    expected_acked_msg_type
                )) {
                return true;
            }

            Serial.println("[GATEWAY_COMM] Packet is not expected ACK");
        } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
            // Normal case.
        } else {
            Serial.print("[GATEWAY_COMM] RX error while waiting ACK, code=");
            Serial.println(state);
        }
    }

    return false;
}

bool WedsGatewayComm::isExpectedAck(
    const WedsPacket& packet,
    uint32_t expected_src_node_id,
    uint16_t expected_acked_sequence_id,
    uint8_t expected_acked_msg_type
) {
    if (packet.header.msg_type != WEDS_MSG_ACK) {
        return false;
    }

    if (packet.header.src_node_id != expected_src_node_id) {
        return false;
    }

    if (packet.header.dst_node_id != WEDS_GATEWAY_ID) {
        return false;
    }

    WedsAckPayload ack;

    bool ok = weds_decode_ack_payload(
        packet.payload,
        packet.header.payload_len,
        ack
    );

    if (!ok) {
        Serial.println("[GATEWAY_COMM] Failed to decode ACK payload");
        return false;
    }

    Serial.print("[GATEWAY_COMM] ACK payload: acked_seq=");
    Serial.print(ack.acked_sequence_id);

    Serial.print(" acked_msg_type=");
    Serial.print(messageTypeName(ack.acked_msg_type));

    Serial.print(" status=");
    Serial.println(ack.status_code);

    if (ack.acked_sequence_id != expected_acked_sequence_id) {
        return false;
    }

    if (ack.acked_msg_type != expected_acked_msg_type) {
        return false;
    }

    if (ack.status_code != WEDS_ACK_STATUS_OK) {
        return false;
    }

    return true;
}

bool WedsGatewayComm::sendAlertModeEnableReliable(
    uint32_t dst_node_id,
    const WedsAlertModeEnablePayload& command
) {
    const uint16_t command_sequence_id = gateway_sequence_id_++;

    WedsPacket command_packet;

    bool ok = weds_build_alert_mode_enable_packet(
        dst_node_id,
        command_sequence_id,
        command,
        command_packet
    );

    if (!ok) {
        Serial.println("[GATEWAY_COMM] Failed to build ALERT_MODE_ENABLE packet");
        return false;
    }

    for (uint8_t attempt = 1; attempt <= WEDS_GATEWAY_COMMAND_MAX_RETRIES; ++attempt) {
        Serial.println();

        Serial.print("[GATEWAY_COMM] ALERT_MODE_ENABLE attempt ");
        Serial.print(attempt);
        Serial.print("/");
        Serial.print(WEDS_GATEWAY_COMMAND_MAX_RETRIES);
        Serial.print(" dst=");
        Serial.print(dst_node_id);
        Serial.print(" seq=");
        Serial.println(command_sequence_id);

        bool tx_ok = sendPacket(command_packet);

        if (tx_ok) {
            bool ack_ok = waitForAck(
                dst_node_id,
                command_sequence_id,
                WEDS_MSG_ALERT_MODE_ENABLE,
                WEDS_GATEWAY_ACK_TIMEOUT_MS
            );

            if (ack_ok) {
                Serial.println("[GATEWAY_COMM] ALERT_MODE_ENABLE confirmed by ACK");
                return true;
            }

            Serial.println("[GATEWAY_COMM] ALERT_MODE_ENABLE ACK timeout");
        }

        if (attempt < WEDS_GATEWAY_COMMAND_MAX_RETRIES) {
            delay(WEDS_GATEWAY_COMMAND_BACKOFF_MS);
        }
    }

    Serial.println("[GATEWAY_COMM] ALERT_MODE_ENABLE not confirmed");
    return false;
}

void WedsGatewayComm::deliverPendingCommandIfAny(uint32_t node_id) {
    WedsAlertModeEnablePayload command;
    if (registry_ == nullptr ||
        !registry_->hasPendingAlertCommand(node_id, command)) {
        return;
    }

    Serial.println();
    Serial.print("[GATEWAY_COMM] Pending command found for node=");
    Serial.println(node_id);

    Serial.println("[GATEWAY_COMM] Trying to deliver pending ALERT_MODE_ENABLE...");

    bool delivered = sendAlertModeEnableReliable(
        node_id,
        command
    );

    if (delivered) {
        registry_->clearPendingAlertCommand(node_id);

        Serial.print("[GATEWAY_COMM] Pending command delivered and cleared for node=");
        Serial.println(node_id);
    } else {
        Serial.print("[GATEWAY_COMM] Pending command still not delivered for node=");
        Serial.println(node_id);
    }
}


