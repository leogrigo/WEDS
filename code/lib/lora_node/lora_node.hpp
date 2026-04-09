#pragma once

#include <Arduino.h>
#include "states.hpp"

namespace LoraComm {

void begin();
bool wake();
void sleep();
bool isAwake();
bool sendMessage(const String& payload, bool keepRadioOn = false);
bool sendNodeStatus(const node_state_t status, bool keepRadioOn = false);

}  // namespace LoraComm
