#pragma once

#include <stdint.h>

// Radio profile for the WEDS deployment area (EU/Italy 868 MHz ISM band).
static constexpr float WEDS_LORA_FREQUENCY_MHZ = 868.0f;
static constexpr float WEDS_LORA_BANDWIDTH_KHZ = 125.0f;
static constexpr uint8_t WEDS_LORA_SPREADING_FACTOR = 7;
static constexpr uint8_t WEDS_LORA_CODING_RATE = 5;
static constexpr uint8_t WEDS_LORA_SYNC_WORD = 0x12;
static constexpr int8_t WEDS_LORA_OUTPUT_POWER_DBM = 14;
static constexpr uint16_t WEDS_LORA_PREAMBLE_LENGTH = 8;
