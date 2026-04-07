#pragma once

#include <Arduino.h>

namespace LoraGateway {

void begin();
bool available();
bool receiveMessage(String& message);

}  // namespace LoraGateway
