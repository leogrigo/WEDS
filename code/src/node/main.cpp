#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "WedsNodeComm.h"
#include "WedsNodeState.h"
#include "WedsAnomalyDetector.h"
#include "WedsNodeSimulation.h"
#include "WedsRiskScore.h"
#include "WedsSensors.h"
#include "WedsNodeConfig.h"

namespace {

constexpr uint32_t NODE_RX_WINDOW_BIT = 1UL << 0;

WedsNodeComm nodeComm;
WedsNodeState nodeState;
WedsAnomalyDetector anomalyDetector;
WedsRiskScoreCalculator riskScoreCalculator;

EventGroupHandle_t nodeEvents = nullptr;
SemaphoreHandle_t commMutex = nullptr;
SemaphoreHandle_t stateMutex = nullptr;

void fatalError(const char* message) {
    Serial.print("[NODE_FATAL] ");
    Serial.println(message);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(WEDS_NODE_ERROR_TASK_DELAY_MS));
    }
}

void lockComm() {
    if (commMutex != nullptr) {
        xSemaphoreTake(commMutex, portMAX_DELAY);
    }
}

void unlockComm() {
    if (commMutex != nullptr) {
        xSemaphoreGive(commMutex);
    }
}

void lockState() {
    if (stateMutex != nullptr) {
        xSemaphoreTake(stateMutex, portMAX_DELAY);
    }
}

void unlockState() {
    if (stateMutex != nullptr) {
        xSemaphoreGive(stateMutex);
    }
}

WedsSensorSample readSensors() {
    if (WEDS_NODE_SENSING_MODE == WEDS_NODE_ENVIRONMENT_SENSING) {
        return weds_read_environment_sample();
    }

    return weds_read_simulated_sample();
}

bool isAlertPayload(const WedsNodeStatusPayload& payload) {
    return payload.anomaly_state == WEDS_DETECTION_ALERT ||
        payload.risk_state == WEDS_DETECTION_ALERT;
}

bool sendPayload(const WedsNodeStatusPayload& payload) {
    lockComm();
    const bool sent = isAlertPayload(payload)
        ? nodeComm.sendAlert(payload)
        : nodeComm.sendStatus(payload);
    unlockComm();

    return sent;
}

void NodeRxTask(void* pvParameters) {
    (void)pvParameters;

    for (;;) {
        xEventGroupWaitBits(
            nodeEvents,
            NODE_RX_WINDOW_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY
        );

        while ((xEventGroupGetBits(nodeEvents) & NODE_RX_WINDOW_BIT) != 0) {
            WedsAlertModeEnablePayload command{};

            lockComm();
            const bool received = nodeComm.pollAlertModeEnable(
                command,
                WEDS_NODE_RX_POLL_CHUNK_MS
            );
            unlockComm();

            if (received) {
                lockState();
                nodeState.applyAlertModeCommand(command);
                unlockState();
            }

            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

void NodeCycleTask(void* pvParameters) {
    (void)pvParameters;

    for (;;) {
        Serial.println();

        lockState();
        nodeState.refreshAlertMode();
        nodeState.setSleepEnabled(false);
        unlockState();

        const uint32_t rx_window_start_ms = millis();
        xEventGroupSetBits(nodeEvents, NODE_RX_WINDOW_BIT);

        const WedsSensorSample sample = readSensors();
        const WedsAnomalyResult anomaly = anomalyDetector.update(sample);
        printAnomalyResults(anomaly);

        const WedsRiskResult risk = riskScoreCalculator.update(sample);

        lockState();
        const WedsNodeStatusPayload payload = nodeState.buildPayload(sample, anomaly, risk);
        unlockState();

        const uint32_t elapsed_rx_ms = millis() - rx_window_start_ms;
        if (elapsed_rx_ms < WEDS_NODE_MIN_RX_WINDOW_MS) {
            vTaskDelay(pdMS_TO_TICKS(WEDS_NODE_MIN_RX_WINDOW_MS - elapsed_rx_ms));
        }

        xEventGroupClearBits(nodeEvents, NODE_RX_WINDOW_BIT);

        const bool sent = sendPayload(payload);
        if (!sent) {
            Serial.println("[NODE] Failed to send payload to gateway");
        }

        lockState();
        nodeState.refreshAlertMode();
        const bool sleep_enabled = !nodeState.alertModeActive();
        nodeState.setSleepEnabled(sleep_enabled);
        const uint32_t next_sample_delay_ms = nodeState.sampleIntervalMs();
        unlockState();

        if (sleep_enabled) {
            lockComm();
            nodeComm.sleepRadio();
            unlockComm();
        }

        vTaskDelay(pdMS_TO_TICKS(next_sample_delay_ms));
    }
}

void createNodeTasks() {
    BaseType_t ok = xTaskCreatePinnedToCore(
        NodeRxTask,
        "NodeRxTask",
        WEDS_NODE_RX_TASK_STACK_BYTES,
        nullptr,
        WEDS_NODE_RX_TASK_PRIORITY,
        nullptr,
        WEDS_NODE_RX_TASK_CORE
    );

    if (ok != pdPASS) {
        fatalError("Failed to create NodeRxTask");
    }

    ok = xTaskCreatePinnedToCore(
        NodeCycleTask,
        "NodeCycleTask",
        WEDS_NODE_CYCLE_TASK_STACK_BYTES,
        nullptr,
        WEDS_NODE_CYCLE_TASK_PRIORITY,
        nullptr,
        WEDS_NODE_CYCLE_TASK_CORE
    );

    if (ok != pdPASS) {
        fatalError("Failed to create NodeCycleTask");
    } else {
        Serial.println("[NODE] FreeRTOS tasks created successfully");
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(WEDS_NODE_BOOT_DELAY_MS);

    Serial.println();
    Serial.println("=== WEDS Node Firmware ===");

    nodeEvents = xEventGroupCreate();
    if (nodeEvents == nullptr) {
        fatalError("Failed to create node event group");
    }

    commMutex = xSemaphoreCreateMutex();
    if (commMutex == nullptr) {
        fatalError("Failed to create communication mutex");
    }

    stateMutex = xSemaphoreCreateMutex();
    if (stateMutex == nullptr) {
        fatalError("Failed to create state mutex");
    }

    if (WEDS_NODE_SENSING_MODE == WEDS_NODE_ENVIRONMENT_SENSING) {
        weds_sensors_begin();
    }

    randomSeed(micros());

    lockComm();
    const bool commReady = nodeComm.begin();
    unlockComm();

    if (!commReady) {
        fatalError("Node communication init failed");
    }

    lockState();
    nodeState.begin(nodeComm.getNodeId());
    unlockState();

    createNodeTasks();
    Serial.println("[NODE] FreeRTOS tasks started");
}

void loop() {
    lockComm();
    nodeComm.loop();
    unlockComm();

    vTaskDelay(pdMS_TO_TICKS(WEDS_NODE_LOOP_IDLE_DELAY_MS));
}
