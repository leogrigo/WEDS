#pragma once

#include <stdint.h>

/** @brief LoRa frequency for the WEDS deployment area (EU/Italy 868 MHz ISM band). */
static constexpr float WEDS_LORA_FREQUENCY_MHZ = 868.0f;

/** @brief LoRa bandwidth setting in kHz. */
static constexpr float WEDS_LORA_BANDWIDTH_KHZ = 125.0f;

/** @brief LoRa spreading factor. */
static constexpr uint8_t WEDS_LORA_SPREADING_FACTOR = 7;

/** @brief LoRa coding rate denominator. */
static constexpr uint8_t WEDS_LORA_CODING_RATE = 5;

/** @brief LoRa synchronization word. */
static constexpr uint8_t WEDS_LORA_SYNC_WORD = 0x12;

/** @brief LoRa transmission output power in dBm. */
static constexpr int8_t WEDS_LORA_OUTPUT_POWER_DBM = 14;

/** @brief LoRa preamble length. */
static constexpr uint16_t WEDS_LORA_PREAMBLE_LENGTH = 8;
