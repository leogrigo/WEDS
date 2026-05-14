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

void GatewayRadioTask(void* pvParameters) {
    (void)pvParameters;

    for (;;) {
        gatewayComm.loop();

        lockRegistry();
        gatewayComm.poll();
        unlockRegistry();

        vTaskDelay(pdMS_TO_TICKS(WEDS_GATEWAY_RADIO_TASK_DELAY_MS));
    }
}

void GatewayApiTask(void* pvParameters) {
    (void)pvParameters;

    for (;;) {
        lockRegistry();
        gatewayApi.handleClient();
        unlockRegistry();

        vTaskDelay(pdMS_TO_TICKS(WEDS_GATEWAY_API_TASK_DELAY_MS));
    }
}

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

void loop() {
    vTaskDelay(pdMS_TO_TICKS(WEDS_GATEWAY_LOOP_IDLE_DELAY_MS));
}



