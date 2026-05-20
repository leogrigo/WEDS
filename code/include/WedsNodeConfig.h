#pragma once

#include <stdint.h>

#include "WedsRadioConfig.h"

/**
 * @brief Defines the operational mode for node sensing.
 */
enum WedsNodeSensingMode : uint8_t {
    /** @brief Node operates using actual environment sensors. */
    WEDS_NODE_ENVIRONMENT_SENSING = 1,
    /** @brief Node operates in simulation mode. */
    WEDS_NODE_SIMULATION = 0
};

/**
 * @brief Defines various simulation scenarios for node testing.
 */
enum WedsSimulationMode : uint8_t {
    /** @brief Normal environmental conditions simulation. */
    WEDS_SIM_MODE_NORMAL,
    /** @brief Simulated prolonged dry period. */
    WEDS_SIM_MODE_DRY_PERIOD,
    /** @brief Simulated sudden drop in gas concentration. */
    WEDS_SIM_MODE_GAS_DROP,
    /** @brief Simulated fire event. */
    WEDS_SIM_MODE_FIRE_EVENT,
    /** @brief Simulated sensor failure or fault. */
    WEDS_SIM_MODE_SENSOR_FAULT
};

/** @brief Selected sensing mode for the node. */
static constexpr WedsNodeSensingMode WEDS_NODE_SENSING_MODE =
    WEDS_NODE_ENVIRONMENT_SENSING;

/** @brief Selected simulation scenario for the node. */
static constexpr WedsSimulationMode WEDS_SELECTED_SIMULATION_MODE =
    WEDS_SIM_MODE_FIRE_EVENT;

/** @brief Default interval between sensor samples in milliseconds. */
static constexpr uint32_t WEDS_NODE_DEFAULT_SAMPLE_INTERVAL_MS = 2000;

/** @brief Timeout for node-to-gateway acknowledgment in milliseconds. */
static constexpr uint32_t WEDS_NODE_ACK_TIMEOUT_MS = 1500;

/** @brief Maximum number of retry attempts for alert delivery. */
static constexpr uint8_t WEDS_NODE_ALERT_MAX_RETRIES = 3;

/** @brief Delay before retrying an alert transmission in milliseconds. */
static constexpr uint32_t WEDS_NODE_RETRY_BACKOFF_MS = 300;

/** @brief Delay after node boot before starting operations in milliseconds. */
static constexpr uint32_t WEDS_NODE_BOOT_DELAY_MS = 500;

/** @brief Delay inside the node error handling task in milliseconds. */
static constexpr uint32_t WEDS_NODE_ERROR_TASK_DELAY_MS = 1000;

/** @brief Delay during idle loops in the main task in milliseconds. */
static constexpr uint32_t WEDS_NODE_LOOP_IDLE_DELAY_MS = 1000;

/** @brief Minimum duration for the receive window in milliseconds. */
static constexpr uint32_t WEDS_NODE_MIN_RX_WINDOW_MS = 250;

/** @brief Polling chunk duration during receive operations in milliseconds. */
static constexpr uint32_t WEDS_NODE_RX_POLL_CHUNK_MS = 50;

/** @brief Stack size allocated for the node cycle task in bytes. */
static constexpr uint32_t WEDS_NODE_CYCLE_TASK_STACK_BYTES = 8192;

/** @brief Stack size allocated for the receive task in bytes. */
static constexpr uint32_t WEDS_NODE_RX_TASK_STACK_BYTES = 8192;

/** @brief Priority level for the node cycle task. */
static constexpr uint8_t WEDS_NODE_CYCLE_TASK_PRIORITY = 2;

/** @brief Priority level for the receive task. */
static constexpr uint8_t WEDS_NODE_RX_TASK_PRIORITY = 3;

/** @brief Core assignment for the node cycle task. */
static constexpr int8_t WEDS_NODE_CYCLE_TASK_CORE = 1;

/** @brief Core assignment for the receive task. */
static constexpr int8_t WEDS_NODE_RX_TASK_CORE = 0;

/** @brief Pin number used for Heltec battery control. */
static constexpr uint8_t WEDS_NODE_HELTEC_VBAT_CTRL_PIN = 37;

/** @brief Pin number used for Heltec battery voltage ADC reading. */
static constexpr uint8_t WEDS_NODE_HELTEC_VBAT_ADC_PIN = 1;

/** @brief Conversion factor from ADC reading to battery voltage. */
static constexpr float WEDS_NODE_HELTEC_ADC_TO_VBAT = 1.0f / 238.7f;

/** @brief Voltage representing an empty battery. */
static constexpr float WEDS_NODE_BATTERY_EMPTY_V = 3.04f;

/** @brief Voltage representing a fully charged battery. */
static constexpr float WEDS_NODE_BATTERY_FULL_V = 4.26f;

/** @brief Conversion factor from voltage to battery percentage. */
static constexpr float WEDS_NODE_BATTERY_PERCENT_PER_V =
    100.0f / (WEDS_NODE_BATTERY_FULL_V - WEDS_NODE_BATTERY_EMPTY_V);
