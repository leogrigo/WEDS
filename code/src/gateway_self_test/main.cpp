#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <WiFi.h>
#include <time.h>

#include "WedsGatewayConfig.h"
#include "WedsGatewayMqtt.h"
#include "WedsGatewayRegistry.h"
#include "secrets.h"

SET_LOOP_TASK_STACK_SIZE(24 * 1024);

namespace {

WedsGatewayRegistry registry;
WedsGatewayMqtt gatewayMqtt;
SemaphoreHandle_t registryMutex = nullptr;

uint16_t selfTestSequenceIds[WEDS_SELF_TEST_NODE_COUNT] = {1, 1, 1, 1};
bool selfTestNodeAAlertActive = false;

void fatalError(const char* message) {
    Serial.print("[FATAL] ");
    Serial.println(message);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(WEDS_GATEWAY_ERROR_TASK_DELAY_MS));
    }
}

void lockRegistry() {
    if (registryMutex != nullptr) {
        xSemaphoreTake(registryMutex, portMAX_DELAY);
    }
}

void unlockRegistry() {
    if (registryMutex != nullptr) {
        xSemaphoreGive(registryMutex);
    }
}

bool connectWifi() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    Serial.print("[GATEWAY_WIFI] Connecting SSID=");
    Serial.println(secret_wifi);

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(WEDS_WIFI_MODEM_SLEEP_ENABLED);
    WiFi.begin(secret_wifi, secret_password);

    const uint32_t start_ms = millis();
    uint32_t last_print_ms = 0;

    while (WiFi.status() != WL_CONNECTED &&
           millis() - start_ms < WEDS_WIFI_CONNECT_TIMEOUT_MS) {
        const uint32_t now_ms = millis();
        if (now_ms - last_print_ms >= WEDS_WIFI_CONNECT_PRINT_INTERVAL_MS) {
            Serial.print(".");
            last_print_ms = now_ms;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[GATEWAY_WIFI] Connection failed");
        return false;
    }

    Serial.print("[GATEWAY_WIFI] Connected IP=");
    Serial.println(WiFi.localIP());
    return true;
}

void syncClock() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    Serial.println("[GATEWAY_WIFI] Syncing clock with NTP...");
    configTime(0, 0, WEDS_NTP_SERVER_1, WEDS_NTP_SERVER_2);

    const uint32_t start_ms = millis();
    time_t now = time(nullptr);
    while (now < WEDS_MIN_VALID_EPOCH_S &&
           millis() - start_ms < WEDS_NTP_SYNC_TIMEOUT_MS) {
        vTaskDelay(pdMS_TO_TICKS(WEDS_NTP_SYNC_POLL_MS));
        now = time(nullptr);
    }

    if (now >= WEDS_MIN_VALID_EPOCH_S) {
        Serial.print("[GATEWAY_WIFI] NTP synced epoch_s=");
        Serial.println(static_cast<unsigned long>(now));
    } else {
        Serial.println("[GATEWAY_WIFI] NTP sync timeout");
    }
}

WedsNodeStatusPayload createNormalStatus(
    uint32_t timestampS,
    float temperature,
    float humidity,
    float pressure,
    float gasResistance,
    float batteryLevel,
    float anomalyScore,
    float riskScore
) {
    WedsNodeStatusPayload status;
    status.timestamp_s = timestampS;
    status.temperature = temperature;
    status.humidity = humidity;
    status.pressure = pressure;
    status.gas_resistance = gasResistance;
    status.battery_level = batteryLevel;
    status.anomaly_state = WEDS_DETECTION_NORMAL;
    status.anomaly_score = anomalyScore;
    status.risk_state = WEDS_DETECTION_NORMAL;
    status.risk_score = riskScore;
    return status;
}

WedsNodeStatusPayload createAlertStatus(uint32_t timestampS) {
    WedsNodeStatusPayload status;
    status.timestamp_s = timestampS;
    status.temperature = 48.0f + static_cast<float>(timestampS % 4) * 0.3f;
    status.humidity = 18.0f - static_cast<float>(timestampS % 3) * 0.2f;
    status.pressure = 1011.7f;
    status.gas_resistance = 21000.0f + static_cast<float>(timestampS % 5) * 120.0f;
    status.battery_level = 86.0f;
    status.anomaly_state = WEDS_DETECTION_ALERT;
    status.anomaly_score = 0.93f;
    status.risk_state = WEDS_DETECTION_ALERT;
    status.risk_score = 0.88f;
    return status;
}

size_t selfTestIndexForNode(uint32_t nodeId) {
    for (size_t i = 0; i < WEDS_SELF_TEST_NODE_COUNT; ++i) {
        if (WEDS_SELF_TEST_NODES[i].node_id == nodeId) {
            return i;
        }
    }

    return 0;
}

WedsNodeStatusPayload createSelfTestStatus(uint32_t nodeId, uint32_t timestampS) {
    const uint32_t phaseS = timestampS % 60;
    const bool nodeAAlert = nodeId == WEDS_SELF_TEST_NODES[0].node_id && phaseS >= 20 && phaseS <= 40;

    if (nodeAAlert) {
        return createAlertStatus(timestampS);
    }

    const size_t index = selfTestIndexForNode(nodeId);
    const float wave = static_cast<float>((timestampS + index * 7) % 10);
    const float baseTemperature = 29.5f + static_cast<float>(index) * 0.9f;
    const float baseHumidity = 46.0f - static_cast<float>(index) * 1.5f;
    const float baseRisk = 0.14f + static_cast<float>(index) * 0.04f;

    return createNormalStatus(
        timestampS,
        baseTemperature + wave * 0.12f,
        baseHumidity - wave * 0.10f,
        1013.0f + static_cast<float>((timestampS + index) % 4) * 0.2f,
        85000.0f - wave * 350.0f - static_cast<float>(index) * 900.0f,
        92.0f - static_cast<float>((timestampS / 60) % 5) * 0.2f - static_cast<float>(index),
        0.08f + wave * 0.01f,
        baseRisk + wave * 0.01f
    );
}

void updateFakeNode(size_t index, uint32_t timestampS) {
    const uint32_t nodeId = WEDS_SELF_TEST_NODES[index].node_id;
    const WedsNodeStatusPayload status = createSelfTestStatus(nodeId, timestampS);
    const uint8_t msgType =
        (status.anomaly_state == WEDS_DETECTION_ALERT ||
         status.risk_state == WEDS_DETECTION_ALERT)
            ? WEDS_MSG_NODE_ALERT
            : WEDS_MSG_NODE_STATUS;

    registry.updateNodeStatus(
        nodeId,
        selfTestSequenceIds[index]++,
        msgType,
        status
    );

    if (msgType == WEDS_MSG_NODE_ALERT) {
        registry.createAlertCommandsForNeighbors(nodeId, status);
    }
}

void simulateGatewaySelfTestData() {
    const uint32_t timestampS = millis() / 1000;
    const uint32_t phaseS = timestampS % 60;
    const bool nodeAAlert = phaseS >= 20 && phaseS <= 40;

    if (nodeAAlert != selfTestNodeAAlertActive) {
        selfTestNodeAAlertActive = nodeAAlert;
        Serial.print("[SELF_TEST] NODE_A ");
        Serial.println(nodeAAlert ? "entered alert phase" : "left alert phase");
    }

    lockRegistry();
    for (size_t i = 0; i < WEDS_SELF_TEST_NODE_COUNT; ++i) {
        updateFakeNode(i, timestampS);
    }

    Serial.print("[SELF_TEST] Fake update complete, known_nodes=");
    Serial.println(registry.getKnownNodeCount());
    unlockRegistry();
}

void GatewaySelfTestDataTask(void* pvParameters) {
    (void)pvParameters;

    for (;;) {
        simulateGatewaySelfTestData();
        vTaskDelay(pdMS_TO_TICKS(WEDS_SELF_TEST_UPDATE_PERIOD_MS));
    }
}

void GatewayMqttTask(void* pvParameters) {
    (void)pvParameters;

    for (;;) {
        lockRegistry();
        gatewayMqtt.loop();
        unlockRegistry();

        vTaskDelay(pdMS_TO_TICKS(WEDS_GATEWAY_MQTT_TASK_DELAY_MS));
    }
}

void createGatewaySelfTestTasks() {
    BaseType_t ok;

    ok = xTaskCreatePinnedToCore(
        GatewaySelfTestDataTask,
        "GatewaySelfTestDataTask",
        WEDS_GATEWAY_TASK_STACK_BYTES,
        nullptr,
        WEDS_SELF_TEST_DATA_TASK_PRIORITY,
        nullptr,
        WEDS_SELF_TEST_DATA_TASK_CORE
    );

    if (ok != pdPASS) {
        fatalError("Failed to create GatewaySelfTestDataTask");
    }

    ok = xTaskCreatePinnedToCore(
        GatewayMqttTask,
        "GatewayMqttTask",
        WEDS_GATEWAY_MQTT_TASK_STACK_BYTES,
        nullptr,
        WEDS_GATEWAY_MQTT_TASK_PRIORITY,
        nullptr,
        WEDS_GATEWAY_MQTT_TASK_CORE
    );

    if (ok != pdPASS) {
        fatalError("Failed to create GatewayMqttTask");
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println();
    Serial.println("=== WEDS Gateway Self Test Firmware ===");

    registryMutex = xSemaphoreCreateMutex();
    if (registryMutex == nullptr) {
        fatalError("Failed to create registry mutex");
    }

    lockRegistry();
    const bool registryReady = registry.begin();
    unlockRegistry();

    if (!registryReady) {
        fatalError("Gateway registry init failed");
    }

    lockRegistry();
    const bool configLoaded = registry.loadPersistentConfig();
    unlockRegistry();

    if (!configLoaded) {
        Serial.println("[WARN] Gateway persistent config load failed");
    }

    lockRegistry();
    for (size_t i = 0; i < WEDS_SELF_TEST_NODE_COUNT; ++i) {
        if (WEDS_SELF_TEST_NODES[i].location_known) {
            registry.setNodeLocation(
                WEDS_SELF_TEST_NODES[i].node_id,
                WEDS_SELF_TEST_NODES[i].latitude,
                WEDS_SELF_TEST_NODES[i].longitude
            );
        }
    }

    const uint32_t timestampS = millis() / 1000;
    for (size_t i = 0; i < WEDS_SELF_TEST_NODE_COUNT; ++i) {
        updateFakeNode(i, timestampS);
    }
    unlockRegistry();

    if (!connectWifi()) {
        Serial.println("[WARN] WiFi connection failed; MQTT will retry when WiFi is available");
    } else {
        syncClock();
    }

    lockRegistry();
    const bool mqttReady = gatewayMqtt.begin(
        secret_mqtt_host,
        secret_mqtt_port,
        &registry
    );
    unlockRegistry();

    if (!mqttReady) {
        Serial.println("[WARN] Gateway MQTT init failed; continuing without MQTT");
    }

    createGatewaySelfTestTasks();
    Serial.println("[SELF_TEST] FreeRTOS tasks started");
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(WEDS_GATEWAY_LOOP_IDLE_DELAY_MS));
}



