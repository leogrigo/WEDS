#include <Arduino.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>


#include "WedsAnomalyDetector.h"
#include "WedsNodeComm.h"
#include "WedsNodeConfig.h"
#include "WedsNodeSimulation.h"
#include "WedsNodeState.h"
#include "WedsRiskScore.h"
#include "WedsSensors.h"


size_t getArduinoLoopTaskStackSize(void) {
  return WEDS_NODE_CYCLE_TASK_STACK_BYTES;
}

namespace {

WedsNodeComm nodeComm;
WedsNodeState nodeState;
WedsAnomalyDetector anomalyDetector;
WedsRiskScoreCalculator riskScoreCalculator;

constexpr EventBits_t WEDS_CYCLE_SAMPLE_READY = BIT0;
constexpr EventBits_t WEDS_CYCLE_ANOMALY_READY = BIT1;
constexpr EventBits_t WEDS_CYCLE_RISK_READY = BIT2;
constexpr EventBits_t WEDS_CYCLE_SAMPLE_ERROR = BIT3;
constexpr EventBits_t WEDS_CYCLE_RESULTS_READY =
    WEDS_CYCLE_ANOMALY_READY | WEDS_CYCLE_RISK_READY;

struct WedsNodeCycleContext {
  WedsSensorSample sample;
  WedsAnomalyResult anomaly;
  WedsRiskResult risk;
  EventGroupHandle_t events;
  SemaphoreHandle_t state_mutex;
};

WedsNodeCycleContext cycleContext;

/**
 * @brief Halts the system and blinks an error code if a critical boot failure
 * occurs.
 */
void fatalError(const char *message) {
  Serial.print("[NODE_FATAL] ");
  Serial.println(message);

  for (;;) {
    delay(WEDS_NODE_ERROR_TASK_DELAY_MS);
  }
}

#ifdef WEDS_LEGACY_SENSORS
WedsSensorSample readSensors() {
  if (WEDS_NODE_SENSING_MODE == WEDS_NODE_ENVIRONMENT_SENSING) {
    return weds_read_environment_sample();
  }

  return weds_read_simulated_sample();
}
#else

WedsSensorSample readSensors() {
  if (WEDS_NODE_SENSING_MODE == WEDS_NODE_ENVIRONMENT_SENSING) {
    return weds_read_environment_sample();
  }

  return weds_read_simulated_sample();
}

#endif

bool isAlertPayload(const WedsNodeStatusPayload &payload) {
  return payload.anomaly_state == WEDS_DETECTION_ALERT ||
         payload.risk_state == WEDS_DETECTION_ALERT;
}

bool sendPayload(const WedsNodeStatusPayload &payload) {
  const bool sent = isAlertPayload(payload) ? nodeComm.sendAlert(payload)
                                            : nodeComm.sendStatus(payload);

  return sent;
}

void sleepAfterCycle(uint32_t sleep_sec) {
  nodeComm.sleepRadio();

  if (cycleContext.state_mutex != nullptr) {
    xSemaphoreTake(cycleContext.state_mutex, portMAX_DELAY);
  }
  nodeState.prepareForDeepSleep(sleep_sec);
  if (cycleContext.state_mutex != nullptr) {
    xSemaphoreGive(cycleContext.state_mutex);
  }

  Serial.printf("[NODE] Sleeping for %lu s\n", (unsigned long)sleep_sec);
  Serial.flush();

  esp_deep_sleep((uint64_t)sleep_sec * 1000000ULL);
}

void sampleTask(void *parameter) {
  WedsNodeCycleContext *context =
      static_cast<WedsNodeCycleContext *>(parameter);

  Serial.printf("[NODE_TASK] sample core=%d priority=%u\n", xPortGetCoreID(),
                uxTaskPriorityGet(nullptr));

  context->sample = readSensors();

  xSemaphoreTake(context->state_mutex, portMAX_DELAY);
  context->sample.timestamp = nodeState.get_current_time();
  xSemaphoreGive(context->state_mutex);

  weds_print_sample(context->sample);

  if (!context->sample.valid || isnan(context->sample.temperature) ||
      isnan(context->sample.humidity)) {
    Serial.println("[NODE_WARN] Sensor fault detected (NaN or invalid read)");
    xEventGroupSetBits(context->events, WEDS_CYCLE_SAMPLE_ERROR);
    vTaskDelete(nullptr);
  }

  xEventGroupSetBits(context->events, WEDS_CYCLE_SAMPLE_READY);
  vTaskDelete(nullptr);
}

void anomalyTask(void *parameter) {
  WedsNodeCycleContext *context =
      static_cast<WedsNodeCycleContext *>(parameter);

  Serial.printf("[NODE_TASK] anomaly core=%d priority=%u\n", xPortGetCoreID(),
                uxTaskPriorityGet(nullptr));

  const EventBits_t bits = xEventGroupWaitBits(
      context->events, WEDS_CYCLE_SAMPLE_READY | WEDS_CYCLE_SAMPLE_ERROR,
      pdFALSE, pdFALSE, portMAX_DELAY);

  if ((bits & WEDS_CYCLE_SAMPLE_ERROR) != 0) {
    vTaskDelete(nullptr);
  }

  context->anomaly = anomalyDetector.update(context->sample);
  printAnomalyResults(context->anomaly);

  xSemaphoreTake(context->state_mutex, portMAX_DELAY);
  nodeState.update_alert_mode(context->anomaly);
  xSemaphoreGive(context->state_mutex);

  xEventGroupSetBits(context->events, WEDS_CYCLE_ANOMALY_READY);
  vTaskDelete(nullptr);
}

void riskTask(void *parameter) {
  WedsNodeCycleContext *context =
      static_cast<WedsNodeCycleContext *>(parameter);

  Serial.printf("[NODE_TASK] risk core=%d priority=%u\n", xPortGetCoreID(),
                uxTaskPriorityGet(nullptr));

  const EventBits_t bits = xEventGroupWaitBits(
      context->events, WEDS_CYCLE_SAMPLE_READY | WEDS_CYCLE_SAMPLE_ERROR,
      pdFALSE, pdFALSE, portMAX_DELAY);

  if ((bits & WEDS_CYCLE_SAMPLE_ERROR) != 0) {
    vTaskDelete(nullptr);
  }

  context->risk = riskScoreCalculator.update(context->sample);
  Serial.printf("[NODE] Fire Risk Score: %.2f%%\n",
                context->risk.score * 100.0f);

  xEventGroupSetBits(context->events, WEDS_CYCLE_RISK_READY);
  vTaskDelete(nullptr);
}

void commTask(void *parameter) {
  WedsNodeCycleContext *context =
      static_cast<WedsNodeCycleContext *>(parameter);

  Serial.printf("[NODE_TASK] comm core=%d priority=%u\n", xPortGetCoreID(),
                uxTaskPriorityGet(nullptr));

  for (;;) {
    const EventBits_t bits = xEventGroupGetBits(context->events);

    if ((bits & WEDS_CYCLE_SAMPLE_ERROR) != 0) {
      sleepAfterCycle(5U);
    }

    if ((bits & WEDS_CYCLE_RESULTS_READY) == WEDS_CYCLE_RESULTS_READY) {
      break;
    }

    WedsAlertModeEnablePayload command{};
    if (nodeComm.pollAlertModeEnable(command, WEDS_NODE_RX_POLL_CHUNK_MS)) {
      xSemaphoreTake(context->state_mutex, portMAX_DELAY);
      nodeState.applyAlertModeCommand(command);
      xSemaphoreGive(context->state_mutex);
    }
  }

  WedsNodeStatusPayload payload{};
  xSemaphoreTake(context->state_mutex, portMAX_DELAY);
  payload =
      nodeState.buildPayload(context->sample, context->anomaly, context->risk);
  xSemaphoreGive(context->state_mutex);

  const bool sent = sendPayload(payload);
  if (!sent) {
    Serial.println("[NODE] Failed to send payload to gateway");
  }

  xSemaphoreTake(context->state_mutex, portMAX_DELAY);
  nodeState.setSleepDuration(context->risk);
  const uint32_t sleep_sec = nodeState.sleepDurationSec();
  const bool alert_active = nodeState.alertModeActive();
  const uint32_t current_time = nodeState.get_current_time();
  xSemaphoreGive(context->state_mutex);

  Serial.printf("[NODE] Cycle complete (risk=%.2f alert=%s time=%lu)\n",
                context->risk.score, alert_active ? "yes" : "no",
                (unsigned long)current_time);

  sleepAfterCycle(sleep_sec);
}

} // namespace

void setup() {
  esp_task_wdt_init(30, false);
  Serial.begin(115200);
  delay(WEDS_NODE_BOOT_DELAY_MS);

  Serial.println();
  Serial.println("=== WEDS Node Firmware ===");

  if (WEDS_NODE_SENSING_MODE == WEDS_NODE_ENVIRONMENT_SENSING) {
    weds_sensors_begin();
  }

  randomSeed(micros());

  const bool commReady = nodeComm.begin();
  if (!commReady) {
    fatalError("Node communication init failed");
  }

  nodeState.begin(nodeComm.getNodeId());
  anomalyDetector.begin();

  Serial.println("[NODE] Initializing TinyML Model...");
  if (!riskScoreCalculator.begin()) {
    fatalError("Failed to initialize Risk Score Model");
  }

  cycleContext = {};
  cycleContext.events = xEventGroupCreate();
  cycleContext.state_mutex = xSemaphoreCreateMutex();

  if (cycleContext.events == nullptr || cycleContext.state_mutex == nullptr) {
    fatalError("Failed to allocate node task synchronization");
  }

  BaseType_t ok = xTaskCreatePinnedToCore(
      commTask, "weds_comm", WEDS_NODE_RX_TASK_STACK_BYTES, &cycleContext,
      WEDS_NODE_RX_TASK_PRIORITY, nullptr, WEDS_NODE_RX_TASK_CORE);
  if (ok != pdPASS) {
    fatalError("Failed to create comm task");
  }

  ok = xTaskCreatePinnedToCore(sampleTask, "weds_sample",
                               WEDS_NODE_SAMPLE_TASK_STACK_BYTES, &cycleContext,
                               WEDS_NODE_SAMPLE_TASK_PRIORITY, nullptr,
                               WEDS_NODE_CYCLE_TASK_CORE);
  if (ok != pdPASS) {
    fatalError("Failed to create sample task");
  }

  ok = xTaskCreatePinnedToCore(anomalyTask, "weds_anomaly",
                               WEDS_NODE_ANOMALY_TASK_STACK_BYTES,
                               &cycleContext, WEDS_NODE_ANOMALY_TASK_PRIORITY,
                               nullptr, WEDS_NODE_CYCLE_TASK_CORE);
  if (ok != pdPASS) {
    fatalError("Failed to create anomaly task");
  }

  ok = xTaskCreatePinnedToCore(
      riskTask, "weds_risk", WEDS_NODE_RISK_TASK_STACK_BYTES, &cycleContext,
      WEDS_NODE_RISK_TASK_PRIORITY, nullptr, WEDS_NODE_CYCLE_TASK_CORE);
  if (ok != pdPASS) {
    fatalError("Failed to create risk task");
  }
}

void loop() { vTaskDelete(nullptr); }
