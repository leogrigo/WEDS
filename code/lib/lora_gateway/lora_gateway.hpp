#pragma once

#include <Arduino.h>

namespace LoraGateway {

struct ReceivedPacket {
    String payload;
    float rssi;
    float snr;
};

void begin();
bool available();
bool pollReceive(ReceivedPacket& packet);

}  // namespace LoraGateway
