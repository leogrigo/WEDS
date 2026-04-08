#include "lora_node.hpp"

#include <SPI.h>
#include <RadioLib.h>

namespace {

constexpr float LORA_FREQUENCY_MHZ = 868.0f;
constexpr float LORA_BANDWIDTH_KHZ = 125.0f;
constexpr uint8_t LORA_SPREADING_FACTOR = 9;
constexpr uint8_t LORA_CODING_RATE = 7;
constexpr uint8_t LORA_SYNC_WORD = 0x12;
constexpr int8_t LORA_OUTPUT_POWER_DBM = 10;
constexpr uint16_t LORA_PREAMBLE_LENGTH = 8;

// Heltec WiFi LoRa 32 V3 (SX1262).
constexpr uint8_t PIN_LORA_NSS = 8;
constexpr uint8_t PIN_LORA_DIO1 = 14;
constexpr uint8_t PIN_LORA_NRST = 12;
constexpr uint8_t PIN_LORA_BUSY = 13;
constexpr uint8_t PIN_LORA_SCK = 9;
constexpr uint8_t PIN_LORA_MISO = 11;
constexpr uint8_t PIN_LORA_MOSI = 10;

SX1262 radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_NRST, PIN_LORA_BUSY);

bool radioConfigured = false;
bool radioAwake = false;

bool configureRadio() {
    if (radioConfigured) {
        return true;
    }

    int state = radio.begin(
        LORA_FREQUENCY_MHZ,
        LORA_BANDWIDTH_KHZ,
        LORA_SPREADING_FACTOR,
        LORA_CODING_RATE,
        LORA_SYNC_WORD,
        LORA_OUTPUT_POWER_DBM,
        LORA_PREAMBLE_LENGTH
    );

    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("LoRa init failed, code: ");
        Serial.println(state);
        return false;
    }

    radioConfigured = true;
    radioAwake = true;
    return true;
}

String formatNodeStatus(const LoraComm::NodeStatus& status) {
    char payload[192];
    snprintf(
        payload,
        sizeof(payload),
        "id=%lu,samples=%lu,state=%u,temp=%.2f,hum=%.2f,press=%.2f,gas=%.2f,an=%.2f,risk=%.2f",
        static_cast<unsigned long>(status.nodeId),
        static_cast<unsigned long>(status.sampleCount),
        status.anomalyState,
        status.temperature,
        status.humidity,
        status.pressure,
        status.gasResistance,
        status.anomalyScore,
        status.riskScore
    );
    return String(payload);
}

}  // namespace

namespace LoraComm {

void begin() {
    radioConfigured = false;
    radioAwake = false;

    SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);
    Serial.println("LoRa SPI ready");
}

bool wake() {
    if (radioAwake) {
        return true;
    }

    if (!configureRadio()) {
        return false;
    }

    radioAwake = true;
    Serial.println("LoRa radio ON");
    return true;
}

void sleep() {
    if (!radioConfigured || !radioAwake) {
        return;
    }

    radio.sleep();
    radioAwake = false;
    Serial.println("LoRa radio OFF");
}

bool isAwake() {
    return radioAwake;
}

bool sendMessage(const String& payload, bool keepRadioOn) {
    if (!wake()) {
        return false;
    }

    String txPayload = payload;
    int state = radio.transmit(txPayload);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("LoRa transmit failed, code: ");
        Serial.println(state);

        if (!keepRadioOn) {
            sleep();
        }
        return false;
    }

    Serial.print("LoRa TX -> ");
    Serial.println(payload);

    if (!keepRadioOn) {
        sleep();
    }

    return true;
}

bool sendNodeStatus(const NodeStatus& status, bool keepRadioOn) {
    return sendMessage(formatNodeStatus(status), keepRadioOn);
}

}  // namespace LoraComm
