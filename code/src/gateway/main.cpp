#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "WedsGatewayApi.h"
#include "WedsGatewayConfig.h"
#include "WedsGatewayComm.h"
#include "WedsGatewayRegistry.h"
#include "secrets.h"

SET_LOOP_TASK_STACK_SIZE(24 * 1024);

namespace {

WedsGatewayRegistry registry;
WedsGatewayComm gatewayComm;
WedsGatewayApi gatewayApi;

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
 * @brief FreeRTOS task handling the REST API web server.
 * @param pvParameters Task parameters (unused).
 */
void GatewayApiTask(void* pvParameters) {
    (void)pvParameters;

    for (;;) {
        lockRegistry();
        gatewayApi.handleClient();
        unlockRegistry();

        vTaskDelay(pdMS_TO_TICKS(WEDS_GATEWAY_API_TASK_DELAY_MS));
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
        GatewayApiTask,
        "GatewayApiTask",
        WEDS_GATEWAY_TASK_STACK_BYTES,
        nullptr,
        WEDS_GATEWAY_API_TASK_PRIORITY,
        nullptr,
        WEDS_GATEWAY_API_TASK_CORE
    );

    if (ok != pdPASS) {
        fatalError("Failed to create GatewayApiTask");
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

    lockRegistry();
    const bool apiReady = gatewayApi.begin(secret_wifi, secret_password, &registry);
    unlockRegistry();

    if (!apiReady) {
        fatalError("Gateway REST API init failed");
    }

    lockRegistry();
    const bool commReady = gatewayComm.begin(&registry);
    unlockRegistry();

    if (!commReady) {
        fatalError("Gateway communication init failed");
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



