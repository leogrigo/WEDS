#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <WiFi.h>
#include <time.h>

#include "WedsGatewayConfig.h"
#include "WedsGatewayComm.h"
#include "WedsGatewayMqtt.h"
#include "WedsGatewayRegistry.h"
#include "secrets.h"

SET_LOOP_TASK_STACK_SIZE(24 * 1024);

namespace {

WedsGatewayRegistry registry;
WedsGatewayComm gatewayComm;
WedsGatewayMqtt gatewayMqtt;

SemaphoreHandle_t registryMutex = nullptr;

/**
 * @brief Halts the system and blinks an error code if a critical failure occurs.
 * @param message The error message to print.
 */
void fatalError(const char* message) {
    Serial.print("[FATAL] ");
    Serial.println(message);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(WEDS_GATEWAY_ERROR_TASK_DELAY_MS));
    }
}

/**
 * @brief Acquires the mutex lock for the registry.
 */
void lockRegistry() {
    if (registryMutex != nullptr) {
        xSemaphoreTake(registryMutex, portMAX_DELAY);
    }
}

/**
 * @brief Releases the mutex lock for the registry.
 */
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

/**
 * @brief FreeRTOS task handling radio communication for the gateway.
 * @param pvParameters Task parameters (unused).
 */
void GatewayRadioTask(void* pvParameters) {
    (void)pvParameters;

    for (;;) {
        lockRegistry();
        gatewayComm.loop();
        gatewayComm.poll();
        unlockRegistry();

        vTaskDelay(pdMS_TO_TICKS(WEDS_GATEWAY_RADIO_TASK_DELAY_MS));
    }
}

/**
 * @brief FreeRTOS task handling WEDS MQTT integration.
 * @param pvParameters Task parameters (unused).
 */
void GatewayMqttTask(void* pvParameters) {
    (void)pvParameters;

    for (;;) {
        lockRegistry();
        gatewayMqtt.loop();
        unlockRegistry();

        vTaskDelay(pdMS_TO_TICKS(WEDS_GATEWAY_MQTT_TASK_DELAY_MS));
    }
}

/**
 * @brief Creates and starts the FreeRTOS tasks for the gateway.
 */
void createGatewayTasks() {
    BaseType_t ok = xTaskCreatePinnedToCore(
        GatewayRadioTask,
        "GatewayRadioTask",
        WEDS_GATEWAY_TASK_STACK_BYTES,
        nullptr,
        WEDS_GATEWAY_RADIO_TASK_PRIORITY,
        nullptr,
        WEDS_GATEWAY_RADIO_TASK_CORE
    );

    if (ok != pdPASS) {
        fatalError("Failed to create GatewayRadioTask");
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

/**
 * @brief Main setup function for the gateway firmware.
 */
void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println();
    Serial.println("=== WEDS Gateway Firmware ===");

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

    if (!connectWifi()) {
        Serial.println("[WARN] WiFi connection failed; MQTT will retry when WiFi is available");
    } else {
        syncClock();
    }

    lockRegistry();
    const bool commReady = gatewayComm.begin(&registry);
    unlockRegistry();

    if (!commReady) {
        fatalError("Gateway communication init failed");
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

    createGatewayTasks();
    Serial.println("[GATEWAY] FreeRTOS tasks started");
}

/**
 * @brief Main loop function for the gateway firmware.
 */
void loop() {
    vTaskDelay(pdMS_TO_TICKS(WEDS_GATEWAY_LOOP_IDLE_DELAY_MS));
}



