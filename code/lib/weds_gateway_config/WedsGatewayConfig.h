#pragma once

#include <stddef.h>
#include <stdint.h>

// Radio profile for the WEDS deployment area (EU/Italy 868 MHz ISM band).
static constexpr float WEDS_LORA_FREQUENCY_MHZ = 868.0f;
static constexpr float WEDS_LORA_BANDWIDTH_KHZ = 125.0f;
static constexpr uint8_t WEDS_LORA_SPREADING_FACTOR = 7;
static constexpr uint8_t WEDS_LORA_CODING_RATE = 5;
static constexpr uint8_t WEDS_LORA_SYNC_WORD = 0x12;
static constexpr int8_t WEDS_LORA_OUTPUT_POWER_DBM = 14;
static constexpr uint16_t WEDS_LORA_PREAMBLE_LENGTH = 8;

// Reliable gateway-to-node command delivery.
static constexpr uint32_t WEDS_GATEWAY_ACK_TIMEOUT_MS = 1500;   // Time to wait for an ACK before retrying a command.
static constexpr uint8_t WEDS_GATEWAY_COMMAND_MAX_RETRIES = 2;  // Maximum number of retries for a command before giving up and marking it failed.
static constexpr uint32_t WEDS_GATEWAY_COMMAND_BACKOFF_MS = 300;    // Time to wait between retries to avoid radio congestion and improve chances of success on retry attempts.

// Gateway registry sizing. Keep fixed-size buffers for embedded RAM safety.
static constexpr size_t WEDS_MAX_NODES = 16;
static constexpr size_t WEDS_MAX_EVENTS_PER_NODE = 32;
static constexpr size_t WEDS_TREND_POINTS_PER_NODE = 60;

#ifdef WEDS_TREND_SAMPLE_INTERVAL_SEC_OVERRIDE
static constexpr uint32_t WEDS_TREND_SAMPLE_INTERVAL_SEC =
    WEDS_TREND_SAMPLE_INTERVAL_SEC_OVERRIDE;
#else
static constexpr uint32_t WEDS_TREND_SAMPLE_INTERVAL_SEC = 60;  // WEDS_TREND_POINTS_PER_NODE * WEDS_TREND_SAMPLE_INTERVAL_SEC = max historical data duration per node (e.g. 60 points * 60 sec = 1 hour). Adjust as needed for longer/shorter trend history.
#endif

static constexpr double WEDS_NEIGHBOR_RADIUS_M = 50.0;  // Radius in meters to consider nodes as neighbors for alert mode command propagation. Adjust based on expected node density and communication range.

// Alert-mode command generated for neighbor nodes during an alert event.
static constexpr uint16_t WEDS_ALERT_MODE_DURATION_SEC = 600;
static constexpr uint16_t WEDS_ALERT_MODE_SAMPLING_INTERVAL_SEC = 2;

// Persistent gateway config.
static constexpr const char* WEDS_GATEWAY_CONFIG_PATH = "/weds_config.json";
static constexpr uint32_t WEDS_MIN_VALID_EPOCH_S = 1704067200UL; // 2024-01-01 UTC

// WiFi/API timing.
static constexpr uint16_t WEDS_HTTP_PORT = 80;
static constexpr uint32_t WEDS_WIFI_CONNECT_TIMEOUT_MS = 20000;
static constexpr uint32_t WEDS_WIFI_CONNECT_PRINT_INTERVAL_MS = 500;
static constexpr bool WEDS_WIFI_MODEM_SLEEP_ENABLED = true;
static constexpr uint32_t WEDS_NTP_SYNC_TIMEOUT_MS = 5000;
static constexpr uint32_t WEDS_NTP_SYNC_POLL_MS = 200;
static constexpr const char* WEDS_NTP_SERVER_1 = "pool.ntp.org";
static constexpr const char* WEDS_NTP_SERVER_2 = "time.google.com";

// Gateway FreeRTOS task profile.
static constexpr uint32_t WEDS_GATEWAY_API_TASK_DELAY_MS = 10;
static constexpr uint32_t WEDS_GATEWAY_RADIO_TASK_DELAY_MS = 5;
static constexpr uint32_t WEDS_GATEWAY_ERROR_TASK_DELAY_MS = 1000;
static constexpr uint32_t WEDS_GATEWAY_LOOP_IDLE_DELAY_MS = 1000;
static constexpr uint32_t WEDS_GATEWAY_TASK_STACK_BYTES = 8192;
static constexpr uint8_t WEDS_GATEWAY_RADIO_TASK_PRIORITY = 3;
static constexpr uint8_t WEDS_GATEWAY_API_TASK_PRIORITY = 2;
static constexpr int8_t WEDS_GATEWAY_RADIO_TASK_CORE = 1;
static constexpr int8_t WEDS_GATEWAY_API_TASK_CORE = 0;

// Self-test firmware profile.
static constexpr uint32_t WEDS_SELF_TEST_UPDATE_PERIOD_MS = 2000;
static constexpr uint8_t WEDS_SELF_TEST_DATA_TASK_PRIORITY = 1;
static constexpr int8_t WEDS_SELF_TEST_DATA_TASK_CORE = 1;
static constexpr size_t WEDS_SELF_TEST_NODE_COUNT = 4;

struct WedsSelfTestNodeConfig {
    uint32_t node_id;
    bool location_known;
    double latitude;
    double longitude;
};

static constexpr WedsSelfTestNodeConfig WEDS_SELF_TEST_NODES[WEDS_SELF_TEST_NODE_COUNT] = {
    {100001, true, 41.902800, 12.496400},
    {100002, true, 41.902900, 12.496500},
    {100003, true, 41.904000, 12.498000},
    {100004, false, 0.0, 0.0},
};



