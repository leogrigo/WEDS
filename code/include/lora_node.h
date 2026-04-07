#pragma once

#include <Arduino.h>

namespace LoraComm {

struct NodeStatus {
    uint32_t nodeId;
    uint32_t sampleCount;
    uint8_t anomalyState;
    float temperature;
    float humidity;
    float pressure;
    float gasResistance;
    float anomalyScore;
    float riskScore;
};

void begin();
bool wake();
void sleep();
bool isAwake();
bool sendMessage(const String& payload, bool keepRadioOn = false);
bool sendNodeStatus(const NodeStatus& status, bool keepRadioOn = false);

}  // namespace LoraComm
