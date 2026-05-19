#pragma once

#include <stdint.h>

#include "WedsRadioConfig.h"

enum WedsNodeSensingMode : uint8_t {
    WEDS_NODE_ENVIRONMENT_SENSING = 1,
    WEDS_NODE_SIMULATION = 0
};

enum WedsSimulationMode : uint8_t {
    WEDS_SIM_MODE_NORMAL,
    WEDS_SIM_MODE_DRY_PERIOD,
    WEDS_SIM_MODE_GAS_DROP,
    WEDS_SIM_MODE_FIRE_EVENT,
    WEDS_SIM_MODE_SENSOR_FAULT
};

// Node sensing profile.
static constexpr WedsNodeSensingMode WEDS_NODE_SENSING_MODE =
    WEDS_NODE_ENVIRONMENT_SENSING;
static constexpr WedsSimulationMode WEDS_SELECTED_SIMULATION_MODE =
    WEDS_SIM_MODE_FIRE_EVENT;
static constexpr uint32_t WEDS_NODE_DEFAULT_SAMPLE_INTERVAL_MS = 2000;

// Reliable node-to-gateway alert delivery.
static constexpr uint32_t WEDS_NODE_ACK_TIMEOUT_MS = 1500;
static constexpr uint8_t WEDS_NODE_ALERT_MAX_RETRIES = 3;
static constexpr uint32_t WEDS_NODE_RETRY_BACKOFF_MS = 300;

// Node FreeRTOS task profile.
static constexpr uint32_t WEDS_NODE_BOOT_DELAY_MS = 500;
static constexpr uint32_t WEDS_NODE_ERROR_TASK_DELAY_MS = 1000;
static constexpr uint32_t WEDS_NODE_LOOP_IDLE_DELAY_MS = 1000;
static constexpr uint32_t WEDS_NODE_MIN_RX_WINDOW_MS = 250;
static constexpr uint32_t WEDS_NODE_RX_POLL_CHUNK_MS = 50;
static constexpr uint32_t WEDS_NODE_CYCLE_TASK_STACK_BYTES = 8192;
static constexpr uint32_t WEDS_NODE_RX_TASK_STACK_BYTES = 8192;
static constexpr uint8_t WEDS_NODE_CYCLE_TASK_PRIORITY = 2;
static constexpr uint8_t WEDS_NODE_RX_TASK_PRIORITY = 3;
static constexpr int8_t WEDS_NODE_CYCLE_TASK_CORE = 1;
static constexpr int8_t WEDS_NODE_RX_TASK_CORE = 0;

// Heltec WiFi LoRa 32 V3 battery sensing.
static constexpr uint8_t WEDS_NODE_HELTEC_VBAT_CTRL_PIN = 37;
static constexpr uint8_t WEDS_NODE_HELTEC_VBAT_ADC_PIN = 1;
static constexpr float WEDS_NODE_HELTEC_ADC_TO_VBAT = 1.0f / 238.7f;
static constexpr float WEDS_NODE_BATTERY_EMPTY_V = 3.04f;
static constexpr float WEDS_NODE_BATTERY_FULL_V = 4.26f;
static constexpr float WEDS_NODE_BATTERY_PERCENT_PER_V =
    100.0f / (WEDS_NODE_BATTERY_FULL_V - WEDS_NODE_BATTERY_EMPTY_V);
