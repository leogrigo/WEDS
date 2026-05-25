#pragma once

#include <stddef.h>
#include <stdint.h>

#include "WedsRadioConfig.h"

/** @brief Timeout for gateway-to-node acknowledgment in milliseconds. */
static constexpr uint32_t WEDS_GATEWAY_ACK_TIMEOUT_MS = 1500;

/** @brief Maximum number of retry attempts for command delivery. */
static constexpr uint8_t WEDS_GATEWAY_COMMAND_MAX_RETRIES = 2;

/** @brief Delay before retrying a command transmission in milliseconds. */
static constexpr uint32_t WEDS_GATEWAY_COMMAND_BACKOFF_MS = 300;

/** @brief Maximum number of nodes supported by the gateway. */
static constexpr size_t WEDS_MAX_NODES = 16;

/** @brief Maximum number of queued events per node. */
static constexpr size_t WEDS_MAX_EVENTS_PER_NODE = 32;

/** @brief Number of trend points stored per node. */
static constexpr size_t WEDS_TREND_POINTS_PER_NODE = 60;

#ifdef WEDS_TREND_SAMPLE_INTERVAL_SEC_OVERRIDE
/** @brief Overridden trend sampling interval in seconds. */
static constexpr uint32_t WEDS_TREND_SAMPLE_INTERVAL_SEC =
    WEDS_TREND_SAMPLE_INTERVAL_SEC_OVERRIDE;
#else
/** @brief Default trend sampling interval in seconds. */
static constexpr uint32_t WEDS_TREND_SAMPLE_INTERVAL_SEC = 60;
#endif

/** @brief Radius in meters to consider nodes as neighbors. */
static constexpr double WEDS_NEIGHBOR_RADIUS_M = 50.0;

/** @brief Duration of alert mode for neighbor nodes in seconds. */
static constexpr uint16_t WEDS_ALERT_MODE_DURATION_SEC = 600;

/** @brief Sampling interval during alert mode in seconds. */
static constexpr uint16_t WEDS_ALERT_MODE_SAMPLING_INTERVAL_SEC = 2;

/** @brief File path for storing persistent gateway configuration. */
static constexpr const char* WEDS_GATEWAY_CONFIG_PATH = "/weds_config.json";

/** @brief Minimum valid timestamp (epoch in seconds) to ensure correct NTP sync. */
static constexpr uint32_t WEDS_MIN_VALID_EPOCH_S = 1704067200UL;

/** @brief Timeout for WiFi connection attempts in milliseconds. */
static constexpr uint32_t WEDS_WIFI_CONNECT_TIMEOUT_MS = 20000;

/** @brief Interval between status prints during WiFi connection in milliseconds. */
static constexpr uint32_t WEDS_WIFI_CONNECT_PRINT_INTERVAL_MS = 500;

/** @brief Indicates whether WiFi modem sleep is enabled to save power. */
static constexpr bool WEDS_WIFI_MODEM_SLEEP_ENABLED = true;

/** @brief Timeout for NTP time synchronization in milliseconds. */
static constexpr uint32_t WEDS_NTP_SYNC_TIMEOUT_MS = 5000;

/** @brief Polling interval while waiting for NTP synchronization in milliseconds. */
static constexpr uint32_t WEDS_NTP_SYNC_POLL_MS = 200;

/** @brief Primary NTP server address. */
static constexpr const char* WEDS_NTP_SERVER_1 = "pool.ntp.org";

/** @brief Secondary NTP server address. */
static constexpr const char* WEDS_NTP_SERVER_2 = "time.google.com";

/** @brief Delay within the MQTT task loop in milliseconds. */
static constexpr uint32_t WEDS_GATEWAY_MQTT_TASK_DELAY_MS = 200;

/** @brief Delay between MQTT reconnect attempts in milliseconds. */
static constexpr uint32_t WEDS_GATEWAY_MQTT_RECONNECT_INTERVAL_MS = 5000;

/** @brief Maximum MQTT packet size for gateway payloads. */
static constexpr uint16_t WEDS_GATEWAY_MQTT_PACKET_SIZE = 1536;

/** @brief MQTT socket timeout in seconds, kept short to avoid watchdog stalls. */
static constexpr uint16_t WEDS_GATEWAY_MQTT_SOCKET_TIMEOUT_SEC = 2;

/** @brief Gateway status heartbeat interval over MQTT. */
static constexpr uint32_t WEDS_GATEWAY_MQTT_STATUS_INTERVAL_MS = 10000;

/** @brief Delay within the radio task loop in milliseconds. */
static constexpr uint32_t WEDS_GATEWAY_RADIO_TASK_DELAY_MS = 5;

/** @brief Delay within the error handling task loop in milliseconds. */
static constexpr uint32_t WEDS_GATEWAY_ERROR_TASK_DELAY_MS = 1000;

/** @brief Delay within the main idle loop in milliseconds. */
static constexpr uint32_t WEDS_GATEWAY_LOOP_IDLE_DELAY_MS = 1000;

/** @brief Stack size allocated for gateway tasks in bytes. */
static constexpr uint32_t WEDS_GATEWAY_TASK_STACK_BYTES = 8192;

/** @brief Stack size allocated for the gateway MQTT task in bytes. */
static constexpr uint32_t WEDS_GATEWAY_MQTT_TASK_STACK_BYTES = 12288;

/** @brief Priority level for the gateway radio task. */
static constexpr uint8_t WEDS_GATEWAY_RADIO_TASK_PRIORITY = 3;

/** @brief Priority level for the gateway MQTT task. */
static constexpr uint8_t WEDS_GATEWAY_MQTT_TASK_PRIORITY = 1;

/** @brief Core assignment for the gateway radio task. */
static constexpr int8_t WEDS_GATEWAY_RADIO_TASK_CORE = 1;

/** @brief Core assignment for the gateway MQTT task. */
static constexpr int8_t WEDS_GATEWAY_MQTT_TASK_CORE = 1;

/** @brief Update period for self-test simulated data in milliseconds. */
static constexpr uint32_t WEDS_SELF_TEST_UPDATE_PERIOD_MS = 2000;

/** @brief Priority level for the self-test data task. */
static constexpr uint8_t WEDS_SELF_TEST_DATA_TASK_PRIORITY = 1;

/** @brief Core assignment for the self-test data task. */
static constexpr int8_t WEDS_SELF_TEST_DATA_TASK_CORE = 1;

/** @brief Number of simulated nodes used during self-test. */
static constexpr size_t WEDS_SELF_TEST_NODE_COUNT = 4;

/**
 * @brief Configuration structure for self-test simulated nodes.
 */
struct WedsSelfTestNodeConfig {
    /** @brief Unique identifier for the simulated node. */
    uint32_t node_id;
    /** @brief Flag indicating if the node has a known location. */
    bool location_known;
    /** @brief Latitude coordinate of the node. */
    double latitude;
    /** @brief Longitude coordinate of the node. */
    double longitude;
};

/** @brief Array of configuration profiles for self-test simulated nodes. */
static constexpr WedsSelfTestNodeConfig WEDS_SELF_TEST_NODES[WEDS_SELF_TEST_NODE_COUNT] = {
    {100001, true, 41.902800, 12.496400},
    {100002, true, 41.902900, 12.496500},
    {100003, true, 41.904000, 12.498000},
    {100004, false, 0.0, 0.0},
};
