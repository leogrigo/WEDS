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

String formatNodeStatus(const node_state_t status) {
    String ris = "";
    ris += "node_id: " + String(status.node_id) + ", ";
    ris += "n_sample: " + String(status.samples_seen) + ", ";
    ris += "an_state: " + String(status.an_state == ANOMALY_WARNING ? "WARNING" : "NO_ANOMALY") + ", ";
    ris += "temp: " + String(status.anomaly_last_proc_sample.temp, 2) + ", ";
    ris += "hum: " + String(status.anomaly_last_proc_sample.hum, 2) + ", ";
    ris += "press: " + String(status.anomaly_last_proc_sample.press, 2) + ", ";
    ris += "gas: " + String(status.anomaly_last_proc_sample.gas_r, 2) + ", ";
    ris += "an_score: " + String(status.anomaly_last_result.gas_score, 2) + ", ";
    return ris;
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

bool sendNodeStatus(const node_state_t status, bool keepRadioOn) {
    return sendMessage(formatNodeStatus(status), keepRadioOn);
}

}  // namespace LoraComm
