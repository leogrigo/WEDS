#include "lora_gateway.hpp"

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

constexpr uint8_t PIN_LORA_NSS = 8;
constexpr uint8_t PIN_LORA_DIO1 = 14;
constexpr uint8_t PIN_LORA_NRST = 12;
constexpr uint8_t PIN_LORA_BUSY = 13;
constexpr uint8_t PIN_LORA_SCK = 9;
constexpr uint8_t PIN_LORA_MISO = 11;
constexpr uint8_t PIN_LORA_MOSI = 10;

SX1262 radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_NRST, PIN_LORA_BUSY);

bool radioReady = false;
volatile bool packetReceived = false;

#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void onPacketReceived() {
    packetReceived = true;
}

bool configureRadio() {
    if (radioReady) {
        return true;
    }

    SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);

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
        Serial.print("LoRa gateway init failed, code: ");
        Serial.println(state);
        return false;
    }

    radioReady = true;
    Serial.println("LoRa gateway ready");

    radio.setPacketReceivedAction(onPacketReceived);

    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("LoRa gateway listen failed, code: ");
        Serial.println(state);
        radioReady = false;
        return false;
    }

    Serial.println("LoRa gateway listening");
    return true;
}

}  // namespace

namespace LoraGateway {

void begin() {
    radioReady = false;
    configureRadio();
}

bool available() {
    return configureRadio();
}

bool pollReceive(ReceivedPacket& packet) {
    if (!configureRadio()) {
        return false;
    }

    if (!packetReceived) {
        return false;
    }

    packetReceived = false;
    packet.payload = "";
    int state = radio.readData(packet.payload);

    if (state == RADIOLIB_ERR_NONE) {
        packet.rssi = radio.getRSSI();
        packet.snr = radio.getSNR();
        radio.startReceive();
        return true;
    }

    if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println("LoRa receive CRC mismatch");
    } else {
        Serial.print("LoRa receive failed, code: ");
        Serial.println(state);
    }

    radio.startReceive();

    return false;
}

}  // namespace LoraGateway
