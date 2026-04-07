#include <Arduino.h>

#include "lora.h"

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("Minimal LoRa gateway boot");
    LoraGateway::begin();
}

void loop() {
    String message;

    if (LoraGateway::receiveMessage(message)) {
        Serial.print("LoRa RX <- ");
        Serial.println(message);
    }
}
