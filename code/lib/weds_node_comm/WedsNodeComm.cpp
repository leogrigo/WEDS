#include "WedsNodeComm.h"
#include <esp_sleep.h>
#include <heltec_unofficial.h>
#include "WedsNodeConfig.h"

namespace {

constexpr uint32_t WEDS_NODE_COMM_RTC_MAGIC = 0x574E434DUL;  // "WNCM"
constexpr uint16_t WEDS_NODE_COMM_RTC_VERSION = 1;

struct WedsRtcNodeCommState {
    uint32_t magic;
    uint16_t version;
    uint16_t sequence_id;
    bool has_last_gateway_command;
    uint16_t last_gateway_command_sequence_id;
    uint8_t last_gateway_command_msg_type;
};

RTC_DATA_ATTR WedsRtcNodeCommState rtc_node_comm_state;

bool rtcNodeCommStateValid() {
    return rtc_node_comm_state.magic == WEDS_NODE_COMM_RTC_MAGIC &&
        rtc_node_comm_state.version == WEDS_NODE_COMM_RTC_VERSION &&
        rtc_node_comm_state.sequence_id != 0;
}

void clearRtcNodeCommState() {
    rtc_node_comm_state.magic = WEDS_NODE_COMM_RTC_MAGIC;
    rtc_node_comm_state.version = WEDS_NODE_COMM_RTC_VERSION;
    rtc_node_comm_state.sequence_id = 1;
    rtc_node_comm_state.has_last_gateway_command = false;
    rtc_node_comm_state.last_gateway_command_sequence_id = 0;
    rtc_node_comm_state.last_gateway_command_msg_type = 0;
}

}  // namespace

/**
 * @brief Returns a string representation of the given message type.
 * @param msg_type The message type identifier.
 * @return const char* String representing the message type.
 */
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

WedsNodeComm::WedsNodeComm()
    : node_id_(0),
      sequence_id_(1),
      initialized_(false),
      radio_sleeping_(false),
      has_last_gateway_command_(false),
      last_gateway_command_sequence_id_(0),
      last_gateway_command_msg_type_(0) {}

bool WedsNodeComm::begin() {
    Serial.println("[NODE_COMM] begin()");

    heltec_setup();

    node_id_ = weds_get_node_id_from_mac();

    const bool woke_from_deep_sleep =
        esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED;

    if (!woke_from_deep_sleep || !rtcNodeCommStateValid()) {
        clearRtcNodeCommState();
    }

    sequence_id_ = rtc_node_comm_state.sequence_id;
    has_last_gateway_command_ =
        rtc_node_comm_state.has_last_gateway_command;
    last_gateway_command_sequence_id_ =
        rtc_node_comm_state.last_gateway_command_sequence_id;
    last_gateway_command_msg_type_ =
        rtc_node_comm_state.last_gateway_command_msg_type;

    Serial.print("[NODE_COMM] node_id=");
    Serial.println(node_id_);

    if (!initRadio()) {
        Serial.println("[NODE_COMM] Radio init failed");
        initialized_ = false;
        return false;
    }

    initialized_ = true;
    Serial.println("[NODE_COMM] Ready");

    return true;
}

void WedsNodeComm::loop() {
    heltec_loop();
}

bool WedsNodeComm::initRadio() {
    Serial.println("[NODE_COMM] Initializing LoRa radio...");

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
        radio_sleeping_ = false;
        Serial.println("[NODE_COMM] LoRa init OK");
        return true;
    }

    Serial.print("[NODE_COMM] LoRa init failed, code=");
    Serial.println(state);
    return false;
}

void WedsNodeComm::sleepRadio() {
    if (!initialized_ || radio_sleeping_) {
        return;
    }

    radio.sleep();
    radio_sleeping_ = true;
    Serial.println("[NODE_COMM] LoRa radio sleep");
}

void WedsNodeComm::wakeRadio() {
    if (!initialized_ || !radio_sleeping_) {
        return;
    }

    radio.standby();
    radio_sleeping_ = false;
    Serial.println("[NODE_COMM] LoRa radio wake");
}

bool WedsNodeComm::sendStatus(const WedsNodeStatusPayload& status) {
    if (!initialized_) {
        Serial.println("[NODE_COMM] sendStatus failed: not initialized");
        return false;
    }

    WedsPacket packet;

    const uint16_t status_sequence_id = sequence_id_++;
    persistRtcState();

    bool ok = weds_build_node_status_packet(
        node_id_,
        status_sequence_id,
        status,
        packet
    );

    if (!ok) {
        Serial.println("[NODE_COMM] Failed to build NODE_STATUS packet");
        return false;
    }

    return sendPacket(packet);
}

bool WedsNodeComm::sendAlert(const WedsNodeStatusPayload& status) {
    if (!initialized_) {
        Serial.println("[NODE_COMM] sendAlert failed: not initialized");
        return false;
    }

    const uint16_t alert_sequence_id = sequence_id_++;
    persistRtcState();

    WedsPacket packet;

    bool ok = weds_build_node_alert_packet(
        node_id_,
        alert_sequence_id,
        status,
        packet
    );

    if (!ok) {
        Serial.println("[NODE_COMM] Failed to build NODE_ALERT packet");
        return false;
    }

    for (uint8_t attempt = 1; attempt <= WEDS_NODE_ALERT_MAX_RETRIES; ++attempt) {
        Serial.println();

        Serial.print("[NODE_COMM] NODE_ALERT attempt ");
        Serial.print(attempt);
        Serial.print("/");
        Serial.print(WEDS_NODE_ALERT_MAX_RETRIES);
        Serial.print(" seq=");
        Serial.println(alert_sequence_id);

        bool tx_ok = sendPacket(packet);

        if (!tx_ok) {
            Serial.println("[NODE_COMM] NODE_ALERT TX failed");
        } else {
            Serial.print("[NODE_COMM] Waiting ACK for NODE_ALERT seq=");
            Serial.println(alert_sequence_id);

            bool ack_ok = waitForAck(
                alert_sequence_id,
                WEDS_MSG_NODE_ALERT,
                WEDS_NODE_ACK_TIMEOUT_MS
            );

            if (ack_ok) {
                Serial.println("[NODE_COMM] ACK received, NODE_ALERT confirmed");
                return true;
            }

            Serial.println("[NODE_COMM] ACK timeout");
        }

        if (attempt < WEDS_NODE_ALERT_MAX_RETRIES) {
            Serial.print("[NODE_COMM] Retry backoff ms=");
            Serial.println(WEDS_NODE_RETRY_BACKOFF_MS);
            delay(WEDS_NODE_RETRY_BACKOFF_MS);
        }
    }

    Serial.println("[NODE_COMM] NODE_ALERT failed after max retries");
    return false;
}

bool WedsNodeComm::sendPacket(const WedsPacket& packet) {
    wakeRadio();

    uint8_t buffer[WEDS_MAX_PACKET_SIZE];
    size_t encoded_len = 0;

    bool ok = weds_serialize_packet(
        packet,
        buffer,
        sizeof(buffer),
        encoded_len
    );

    if (!ok) {
        Serial.println("[NODE_COMM] Failed to serialize packet");
        return false;
    }

    Serial.print("[NODE_COMM] TX msg_type=");
    Serial.print(messageTypeName(packet.header.msg_type));

    Serial.print(" seq=");
    Serial.print(packet.header.sequence_id);

    Serial.print(" ack_required=");
    Serial.print(packet.header.ack_required);

    Serial.print(" len=");
    Serial.println(encoded_len);

    int state = radio.transmit(buffer, encoded_len);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("[NODE_COMM] TX OK");
        return true;
    }

    Serial.print("[NODE_COMM] TX failed, code=");
    Serial.println(state);
    return false;
}

bool WedsNodeComm::waitForAck(
    uint16_t expected_acked_sequence_id,
    uint8_t expected_acked_msg_type,
    uint32_t timeout_ms
) {
    wakeRadio();

    const uint32_t start_ms = millis();

    while (millis() - start_ms < timeout_ms) {
        loop();

        uint8_t rx_buffer[WEDS_MAX_PACKET_SIZE];

        int state = radio.receive(rx_buffer, sizeof(rx_buffer));

        if (state == RADIOLIB_ERR_NONE) {
            const size_t len = radio.getPacketLength();

            Serial.print("[NODE_COMM] RX while waiting ACK len=");
            Serial.println(len);

            WedsPacket packet;

            bool ok = weds_deserialize_packet(
                rx_buffer,
                len,
                packet
            );

            if (!ok) {
                Serial.println("[NODE_COMM] Received invalid packet while waiting ACK");
                continue;
            }

            if (isExpectedAck(
                    packet,
                    expected_acked_sequence_id,
                    expected_acked_msg_type
                )) {
                return true;
            }

            Serial.println("[NODE_COMM] Received packet is not the expected ACK");
        } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
            // Normal case while waiting.
        } else {
            Serial.print("[NODE_COMM] RX failed while waiting ACK, code=");
            Serial.println(state);
        }
    }

    return false;
}

bool WedsNodeComm::isExpectedAck(
    const WedsPacket& packet,
    uint16_t expected_acked_sequence_id,
    uint8_t expected_acked_msg_type
) {
    if (packet.header.msg_type != WEDS_MSG_ACK) {
        return false;
    }

    if (packet.header.dst_node_id != node_id_) {
        Serial.print("[NODE_COMM] ACK ignored, dst_node_id=");
        Serial.println(packet.header.dst_node_id);
        return false;
    }

    if (packet.header.src_node_id != WEDS_GATEWAY_ID) {
        Serial.print("[NODE_COMM] ACK ignored, src_node_id=");
        Serial.println(packet.header.src_node_id);
        return false;
    }

    WedsAckPayload ack;

    bool ok = weds_decode_ack_payload(
        packet.payload,
        packet.header.payload_len,
        ack
    );

    if (!ok) {
        Serial.println("[NODE_COMM] Failed to decode ACK payload");
        return false;
    }

    Serial.print("[NODE_COMM] ACK payload: acked_seq=");
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

bool WedsNodeComm::isDuplicateGatewayCommand(const WedsPacket& packet) const {
    return has_last_gateway_command_
        && packet.header.src_node_id == WEDS_GATEWAY_ID
        && packet.header.sequence_id == last_gateway_command_sequence_id_
        && packet.header.msg_type == last_gateway_command_msg_type_;
}

void WedsNodeComm::markGatewayCommandProcessed(const WedsPacket& packet) {
    has_last_gateway_command_ = true;
    last_gateway_command_sequence_id_ = packet.header.sequence_id;
    last_gateway_command_msg_type_ = packet.header.msg_type;
    persistRtcState();

    Serial.println("[NODE_COMM] Gateway command marked as processed");

    Serial.print("[NODE_COMM] seq=");
    Serial.println(packet.header.sequence_id);

    Serial.print("[NODE_COMM] msg_type=");
    Serial.println(messageTypeName(packet.header.msg_type));
}

uint32_t WedsNodeComm::getNodeId() const {
    return node_id_;
}

uint16_t WedsNodeComm::getCurrentSequenceId() const {
    return sequence_id_;
}

void WedsNodeComm::persistRtcState() const {
    rtc_node_comm_state.magic = WEDS_NODE_COMM_RTC_MAGIC;
    rtc_node_comm_state.version = WEDS_NODE_COMM_RTC_VERSION;
    rtc_node_comm_state.sequence_id = sequence_id_ == 0 ? 1 : sequence_id_;
    rtc_node_comm_state.has_last_gateway_command =
        has_last_gateway_command_;
    rtc_node_comm_state.last_gateway_command_sequence_id =
        last_gateway_command_sequence_id_;
    rtc_node_comm_state.last_gateway_command_msg_type =
        last_gateway_command_msg_type_;
}

void WedsNodeComm::printBufferHex(const uint8_t* buffer, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (buffer[i] < 0x10) {
            Serial.print("0");
        }

        Serial.print(buffer[i], HEX);
        Serial.print(" ");
    }

    Serial.println();
}

bool WedsNodeComm::pollAlertModeEnable(
    WedsAlertModeEnablePayload& out_command,
    uint32_t timeout_ms
) {
    if (!initialized_) {
        Serial.println("[NODE_COMM] pollAlertModeEnable failed: not initialized");
        return false;
    }

    wakeRadio();

    const uint32_t start_ms = millis();

    Serial.print("[NODE_COMM] Listening for gateway commands for ");
    Serial.print(timeout_ms);
    Serial.println(" ms");

    while (millis() - start_ms < timeout_ms) {
        loop();

        uint8_t rx_buffer[WEDS_MAX_PACKET_SIZE];

        int state = radio.receive(rx_buffer, sizeof(rx_buffer));

        if (state == RADIOLIB_ERR_NONE) {
            const size_t len = radio.getPacketLength();

            Serial.print("[NODE_COMM] Command packet received len=");
            Serial.println(len);

            WedsPacket packet;

            bool ok = weds_deserialize_packet(
                rx_buffer,
                len,
                packet
            );

            if (!ok) {
                Serial.println("[NODE_COMM] Invalid command packet");
                continue;
            }

            if (packet.header.dst_node_id != node_id_) {
                Serial.print("[NODE_COMM] Command ignored, dst_node_id=");
                Serial.println(packet.header.dst_node_id);
                continue;
            }

            if (packet.header.src_node_id != WEDS_GATEWAY_ID) {
                Serial.print("[NODE_COMM] Command ignored, src_node_id=");
                Serial.println(packet.header.src_node_id);
                continue;
            }

            if (packet.header.msg_type != WEDS_MSG_ALERT_MODE_ENABLE) {
                Serial.print("[NODE_COMM] Unsupported command msg_type=");
                Serial.println(messageTypeName(packet.header.msg_type));
                continue;
            }

            if (isDuplicateGatewayCommand(packet)) {
                Serial.println("[NODE_COMM] Duplicate gateway command detected");

                Serial.print("[NODE_COMM] node_id=");
                Serial.println(node_id_);

                Serial.print("[NODE_COMM] seq=");
                Serial.println(packet.header.sequence_id);

                Serial.print("[NODE_COMM] msg_type=");
                Serial.println(messageTypeName(packet.header.msg_type));

                Serial.println("[NODE_COMM] Re-sending ACK, command ignored");

                if (packet.header.ack_required == WEDS_ACK_REQUIRED) {
                    sendAck(
                        WEDS_GATEWAY_ID,
                        packet.header.sequence_id,
                        packet.header.msg_type,
                        WEDS_ACK_STATUS_OK
                    );
                }

                return false;
            }

            bool payload_ok = weds_decode_alert_mode_enable_payload(
                packet.payload,
                packet.header.payload_len,
                out_command
            );

            if (!payload_ok) {
                Serial.println("[NODE_COMM] Failed to decode ALERT_MODE_ENABLE payload");

                if (packet.header.ack_required == WEDS_ACK_REQUIRED) {
                    sendAck(
                        WEDS_GATEWAY_ID,
                        packet.header.sequence_id,
                        packet.header.msg_type,
                        WEDS_ACK_STATUS_INVALID_PACKET
                    );
                }

                continue;
            }

            Serial.println("[NODE_COMM] ALERT_MODE_ENABLE received");

            Serial.print("[NODE_COMM] alert_source_node_id=");
            Serial.println(out_command.alert_source_node_id);

            Serial.print("[NODE_COMM] duration_sec=");
            Serial.println(out_command.duration_sec);

            Serial.print("[NODE_COMM] sampling_interval_sec=");
            Serial.println(out_command.sampling_interval_sec);

            if (packet.header.ack_required == WEDS_ACK_REQUIRED) {
                sendAck(
                    WEDS_GATEWAY_ID,
                    packet.header.sequence_id,
                    packet.header.msg_type,
                    WEDS_ACK_STATUS_OK
                );
            }

            markGatewayCommandProcessed(packet);

            return true;
        }

        if (state == RADIOLIB_ERR_RX_TIMEOUT) {
            // Normal case.
        } else {
            Serial.print("[NODE_COMM] RX failed while polling command, code=");
            Serial.println(state);
        }
    }

    return false;
}

bool WedsNodeComm::sendAck(
    uint32_t dst_node_id,
    uint16_t acked_sequence_id,
    uint8_t acked_msg_type,
    uint8_t status_code
) {
    WedsPacket ack_packet;

    const uint16_t ack_sequence_id = sequence_id_++;
    persistRtcState();

    bool ok = weds_build_ack_packet(
        node_id_,
        dst_node_id,
        ack_sequence_id,
        acked_sequence_id,
        acked_msg_type,
        status_code,
        ack_packet
    );

    if (!ok) {
        Serial.println("[NODE_COMM] Failed to build ACK packet");
        return false;
    }

    Serial.print("[NODE_COMM] Sending ACK to gateway, acked_seq=");
    Serial.print(acked_sequence_id);

    Serial.print(" acked_msg_type=");
    Serial.println(messageTypeName(acked_msg_type));

    return sendPacket(ack_packet);
}
