# WEDS Firmware — Code Review

Comprehensive review of the WEDS (Wildfire Early Detection System) firmware covering the **node**, **gateway**, **gateway_self_test** firmwares, all 11 shared libraries, and the **DataCollection** project.

---

## Critical Bugs

### 1. `millis()` overflow wraps `alert_mode_until_ms_` — alert mode expires immediately

**File:** [WedsNodeState.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_node_state/WedsNodeState.cpp#L49)

```cpp
alert_mode_until_ms_ = millis() + static_cast<uint32_t>(command.duration_sec) * 1000UL;
```

`millis()` returns a `uint32_t` that wraps every ~49.7 days. If `millis()` is near `UINT32_MAX` and you add `duration_sec * 1000`, the sum silently wraps to a small value.

The `refreshAlertMode()` check uses signed comparison:
```cpp
if (static_cast<int32_t>(millis() - alert_mode_until_ms_) < 0) {
    return;  // still active
}
```

This signed-subtraction trick **is** wrap-safe for durations up to ~24.8 days (`INT32_MAX / 1000` seconds). Since `WEDS_ALERT_MODE_DURATION_SEC = 600` (10 min), the `refreshAlertMode` comparison is actually fine.

**However**, the `applyAlertModeCommand()` stores `alert_mode_until_ms_` which will be a wrapped small value. When `millis()` itself wraps (i.e. goes from ~4.2B to 0), the signed subtraction `(0 - small_value)` would be negative, so it still works. **This is actually OK** given the signed subtraction pattern.

> [!NOTE]
> The signed-subtraction pattern in `refreshAlertMode()` makes this wrap-safe. No fix needed, but a comment documenting this invariant would help future maintainers. The maximum safe duration is ~24.8 days, which is far above the 10-minute alert window.

---

### 2. Gateway radio task: `gatewayComm.loop()` accesses registry without mutex

**File:** [gateway/main.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/src/gateway/main.cpp#L43-L54)

```cpp
void GatewayRadioTask(void* pvParameters) {
    for (;;) {
        gatewayComm.loop();          // ← calls radio.receive() + handleReceivedPacket()
                                      //   which calls registry_->ingestNodeStatus() — NO MUTEX

        lockRegistry();
        gatewayComm.poll();          // ← mutex held, dispatches pending commands
        unlockRegistry();

        vTaskDelay(pdMS_TO_TICKS(WEDS_GATEWAY_RADIO_TASK_DELAY_MS));
    }
}
```

`gatewayComm.loop()` receives LoRa packets and calls `handleReceivedPacket()` → `handleNodeStatusPacket()` which calls `registry_->ingestNodeStatus()` — **modifying the registry without holding `registryMutex`**. Meanwhile, `GatewayApiTask` reads the registry under the same mutex for JSON serialization.

> [!CAUTION]
> This is a data race. The API task may read a partially-updated `WedsNodeRecord` while the radio task is writing to it. On the ESP32 (non-atomic 32-bit writes on 64-bit aligned data), this can cause corrupted readings, invalid JSON, or crashes.

**Fix:**

```diff
  void GatewayRadioTask(void* pvParameters) {
      for (;;) {
+         lockRegistry();
          gatewayComm.loop();
-
-         lockRegistry();
          gatewayComm.poll();
          unlockRegistry();
```

---

### 3. BSEC `nextCall` is in **nanoseconds**, not milliseconds — `weds_sensor_next_call_ms()` returns wrong value

**File:** [WedsSensors.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_sensors/WedsSensors.cpp) (non-legacy branch)

```cpp
int64_t weds_sensor_next_call_ms(){
    if(!checkBmeSensorStatus()) return -1;
    return bmeSensor.nextCall;   // ← nextCall is in NANOSECONDS!
}
```

BSEC's `nextCall` field is in **nanoseconds** since boot. The function name says `_ms` but returns nanoseconds directly.

In [node/main.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/src/node/main.cpp#L76):
```cpp
int64_t time_to_wait = weds_sensor_next_call_ms() - millis();
```

`millis()` returns milliseconds (~0-4.2B), `weds_sensor_next_call_ms()` returns nanoseconds (~0-4.2T). The subtraction produces a **huge positive** number, causing `xTaskDelayUntil` to block for an astronomically long time (effectively forever).

> [!CAUTION]
> This means when using the BME688 (non-legacy sensors), the node will **hang indefinitely** after the first sample because it waits billions of milliseconds for the next call.

**Fix:**
```diff
  int64_t weds_sensor_next_call_ms(){
      if(!checkBmeSensorStatus()) return -1;
-     return bmeSensor.nextCall;
+     return bmeSensor.nextCall / int64_t(1000000);  // Convert ns → ms
  }
```

---

### 4. Zeroed BSEC sample triggers cascading false alerts

**File:** [WedsSensors.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_sensors/WedsSensors.cpp) (non-legacy branch)

```cpp
WedsSensorSample weds_read_environment_sample(){
    WedsSensorSample sample{};          // ← all zeros
    if(bmeSensor.run()) {
        sample.temperature = bmeSensor.temperature;
        // ...
    }
    return sample;                      // ← may return all zeros
}
```

If `bmeSensor.run()` returns false (BSEC not ready, or called before `nextCall`), the function returns a zeroed sample: temperature=0, humidity=0, pressure=0, gas_resistance=0.

This will cause:
- **Anomaly detector**: gas_resistance = 0 is a massive negative residual from the baseline (~18000), producing a huge `gas_score` → instant `WEDS_DETECTION_ALERT`
- **Risk model**: All features at 0 are wildly out-of-distribution → unpredictable risk score
- **Gateway**: Receives an alert, creates `ALERT_MODE_ENABLE` commands for all neighbor nodes

> [!CAUTION]
> A single failed BSEC read can trigger a false fire alert that cascades to neighbor nodes via the gateway's `createAlertCommands` system.

**Fix — add a validity flag to `WedsSensorSample` and check it before processing:**

```diff
  struct WedsSensorSample {
      uint32_t timestamp;
      float temperature;
      float humidity;
      float pressure;
      float gas_resistance;
+     bool valid = false;
  };
```

```diff
  WedsSensorSample weds_read_environment_sample(){
      WedsSensorSample sample{};
      if(bmeSensor.run()) {
          sample.temperature = bmeSensor.temperature;
          // ...
+         sample.valid = true;
      }
      return sample;
  }
```

Then in `NodeCycleTask`, skip anomaly/risk processing when `!sample.valid`.

---

### 5. Pressure units mismatch between legacy and BSEC sensors vs. risk model normalization

**File:** [WedsRiskScore.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_risk_score/WedsRiskScore.cpp#L7-L8)

```cpp
const float means[kNumFeatures] = {18.61777f, 63.66107f, 95955.32f, ...};
const float scales[kNumFeatures] = {6.07850f, 15.99210f, 4519.777f, ...};
```

The pressure normalization mean is **95955 Pa** (~960 hPa), and the scale is **4519 Pa**. This matches raw Pa from the BME688.

Looking at the BSEC sensor code:
```cpp
sample.pressure = bmeSensor.pressure;  // BSEC returns Pa → OK for risk model
```

But the legacy sensor code:
```cpp
sample.pressure = bmp.readPressure();  // BMP280 returns Pa → OK
```

And the legacy print says `Pa`:
```
"[SENSOR] temp=%.2f C hum=%.2f %% pressure=%.2f Pa gas_raw=%.0f"
```

> [!NOTE]
> Actually, both sensor branches deliver **Pa**, and the risk model normalizes with Pa parameters. This is consistent. ✅ No bug here for the main WEDS firmware.
>
> **However**, the simulation code uses values like `pressure = 0.60f` (in [WedsNodeSimulation.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_node_simulation/WedsNodeSimulation.cpp#L35)):
> ```cpp
> float pressure = 0.60f + sampleNormal(0.0f, 0.02f);  // ← ~0.6 kPa??
> ```
> This is wildly wrong for any pressure unit (should be ~101300 Pa, ~1013 hPa, or ~101.3 kPa). When running in simulation mode, the risk model receives `(0.6 - 95955) / 4519 ≈ -21.2` — a massive outlier that corrupts predictions.

> [!WARNING]
> **Simulation mode produces garbage pressure values**, making risk model output meaningless during any simulation test.

**Fix:**
```diff
- float pressure = 0.60f + sampleNormal(0.0f, 0.02f);
+ float pressure = 95000.0f + sampleNormal(0.0f, 500.0f);  // ~950 hPa in Pa
```

---

## High Severity Issues

### 6. Node timestamp is always fake — risk model features are degraded

**File:** [node/main.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/src/node/main.cpp#L165)

```cpp
sample.timestamp = (millis() / 1000) + 86400;
```

The risk model's `update()` uses `sample.timestamp` for time-based bucketing:

```cpp
uint32_t current_day = sample.timestamp / 86400;   // Always ≈ 1 shortly after boot
int bucket_idx = current_day % 7;                    // Always ≈ 1
```

**Impact on `DailyBucket` history:**
- `current_day` starts at 1 and increments by 1 every 86400 seconds (24 hours of uptime)
- The 7-day rolling average (`avgw_temp`, `avgw_rh`) will only ever use 1 bucket for the first 24h
- `temp_delta` and `rh_delta` will be 0 for the first 48h of uptime (no "yesterday" data)
- After 7 days of uptime, the features will be meaningful — but devices typically don't run that long without reset

> [!IMPORTANT]
> For the first 24-48 hours after every boot, 4 of the 7 risk model features (`avgw_temp`, `avgw_rh`, `temp_delta`, `rh_delta`) are essentially constant/zero. The model only receives 3 meaningful features (instantaneous temp, humidity, pressure). This degrades prediction accuracy.

**Fix options:**
1. Use the gateway's NTP-synced time and embed it in LoRa frames
2. Add NTP support to the node (requires WiFi, may not be feasible)
3. Accept degraded accuracy for the first 48h and document it

---

### 7. Node `WEDS_NODE_CYCLE_TASK_STACK_BYTES = 8192` is insufficient for TFLite inference

**File:** [WedsNodeConfig.h](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/include/WedsNodeConfig.h#L38)

```cpp
static constexpr uint32_t WEDS_NODE_CYCLE_TASK_STACK_BYTES = 8192;
```

The `WedsRiskScoreCalculator` class has a **15KB tensor arena as a member variable**:

```cpp
class WedsRiskScoreCalculator {
    // ...
    static const int kTensorArenaSize = 15 * 1024;
    uint8_t tensor_arena_[kTensorArenaSize];  // ← 15KB on the STACK if instantiated locally
};
```

However, `riskScoreCalculator` is declared at namespace scope in `node/main.cpp`:
```cpp
WedsRiskScoreCalculator riskScoreCalculator;  // ← global/static, NOT on stack
```

Since it's a global, the 15KB tensor arena is in BSS, not on the task stack. **But** `NodeCycleTask` still does:
- Sensor I2C reads
- Anomaly EMA computation
- `tflite::MicroInterpreter::Invoke()` — which uses stack for operator scratch buffers
- `buildPayload()` with battery ADC read
- Multiple `Serial.printf()` calls (which use stack for formatting)

With 8KB, this is **tight** for TFLite Micro inference. ESP-IDF's FreeRTOS also uses some of this for task overhead.

> [!WARNING]
> A stack overflow won't crash immediately — FreeRTOS silently corrupts adjacent memory, leading to random crashes hours or days later. Add `uxTaskGetStackHighWaterMark()` monitoring during development.

**Fix:**
```diff
- static constexpr uint32_t WEDS_NODE_CYCLE_TASK_STACK_BYTES = 8192;
+ static constexpr uint32_t WEDS_NODE_CYCLE_TASK_STACK_BYTES = 16384;
```

---

### 8. `weds_read_environment_sample()` (legacy) — no NaN check on sensor reads

**File:** [WedsSensors.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_sensors/WedsSensors.cpp) (legacy `#ifdef WEDS_LEGACY_SENSORS` branch)

The legacy sensor code reads from AHT20 and BMP280 without checking for failures:

```cpp
const float bmp_temperature = bmp.readTemperature();
const float pressure = bmp.readPressure();
aht.getEvent(&humidity, &temp);

sample.temperature = bmp_temperature;  // NaN if I2C failure
sample.pressure = pressure;            // NaN if I2C failure
```

If the I2C bus fails (loose wire, sensor power glitch), these return `NaN`. `NaN` propagates through:
- The EMA filter in the anomaly detector (corrupting `gas_baseline` permanently)
- The risk model (undefined behavior in INT8 quantization: `(int8_t)(NaN)`)

> [!WARNING]
> A single I2C bus glitch can permanently corrupt the anomaly detector's EMA filter state, causing all subsequent readings to produce `NaN` scores until reboot.

**Fix:** Check for `isnan()` and return the previous valid reading or skip the sample.

---

## Medium Severity Issues

### 9. `selfTestIndexForNode()` returns 0 on failure — silent wrong-node mapping

**File:** [gateway_self_test/main.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/src/gateway_self_test/main.cpp#L82-L90)

```cpp
size_t selfTestIndexForNode(uint32_t nodeId) {
    for (size_t i = 0; i < WEDS_SELF_TEST_NODE_COUNT; ++i) {
        if (WEDS_SELF_TEST_NODES[i].node_id == nodeId) {
            return i;
        }
    }
    return 0;   // ← returns index of Node A on failure, not an error
}
```

If called with an unknown `nodeId`, it silently returns 0 (Node A's index). This means any unknown node would get Node A's environmental baseline parameters. In the current code this can't happen since all callers pass known IDs, but it's fragile.

> [!NOTE]
> Consider returning `WEDS_SELF_TEST_NODE_COUNT` as a sentinel and checking the caller, or using `assert()`.

---

### 10. Gateway `dispatchPendingCommands()` — no retry limit or backoff

**File:** [WedsGatewayComm.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_gateway_comm/WedsGatewayComm.cpp) (`dispatchPendingCommands`)

When `sendAlertModeEnable()` fails, the `pending_alert_mode_command` flag stays `true` and the command is retried on the next `poll()` cycle (every 5ms). There's no retry counter, backoff, or timeout.

If the radio is busy or jammed, this creates an **infinite retry loop at 5ms intervals**, monopolizing the radio and preventing normal status reception.

> [!WARNING]
> In a real wildfire scenario, radio congestion from multiple alerting nodes could cause the gateway to enter this tight retry loop, blocking all other LoRa communication.

**Fix:** Add a retry counter and exponential backoff per node entry:
```diff
+ // In WedsNodeRecord or equivalent:
+ uint8_t  command_retry_count = 0;
+ uint32_t command_next_retry_ms = 0;
+
+ static constexpr uint8_t MAX_COMMAND_RETRIES = 5;
```

---

### 11. `readBatteryLevel()` calls `delay(5)` and `pinMode()` under `stateMutex`

**File:** [WedsNodeState.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_node_state/WedsNodeState.cpp#L95-L110)

```cpp
float WedsNodeState::readBatteryLevel() const {
    pinMode(WEDS_NODE_HELTEC_VBAT_CTRL_PIN, OUTPUT);    // ← GPIO reconfiguration
    digitalWrite(WEDS_NODE_HELTEC_VBAT_CTRL_PIN, LOW);
    delay(5);                                             // ← blocking for 5ms

    const float battery_v = /* analogRead ... */;

    pinMode(WEDS_NODE_HELTEC_VBAT_CTRL_PIN, INPUT);      // ← GPIO reconfiguration
    // ...
}
```

This is called from `buildPayload()` which is invoked under `stateMutex`. The 5ms `delay()` + GPIO reconfigurations block the mutex, preventing `NodeRxTask` from processing incoming gateway commands.

More concerning: `pinMode()` is called on every single reading, which is unnecessary and could interfere with other GPIO operations if the pin is shared.

**Fix:** Move `pinMode(OUTPUT)` to `begin()`, read battery outside the locked section, and pass it as a parameter to `buildPayload()`.

---

### 12. Simulation data has no gas_resistance value matching real sensors

**File:** [WedsNodeSimulation.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_node_simulation/WedsNodeSimulation.cpp#L35)

The simulation generates:
```cpp
float gas_resistance = 18000.0f - 80.0f * sinf(0.06f * tick) + sampleNormal(0.0f, 120.0f);
```

The anomaly detector's EMA filter will adapt to ~18000 Ω gas resistance. The risk model normalization mean for `gas_resistance` would need to be checked against the training data — but the 7-feature model doesn't include raw gas. However, pressure at ~0.6 (see issue #5) is still wrong.

> [!NOTE]
> Gas resistance simulation range (~18000 Ω) is reasonable for a BME688 in clean air. However, combined with the wrong pressure value, simulation mode produces unreliable risk scores.

---

## Low Severity / Informational Issues

### 13. `weds_get_node_id_from_mac()` truncates MAC differently than the comm layer

**File:** [WedsProtocol.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_protocol/WedsProtocol.cpp)

```cpp
uint32_t weds_get_node_id_from_mac() {
    uint64_t mac = ESP.getEfuseMac();
    return static_cast<uint32_t>(mac & 0xFFFFFFFF);  // Lower 32 bits
}
```

This simply takes the lower 32 bits of the MAC. If two ESP32s share the same lower 32 bits (unlikely but possible with MAC spoofing or manufacturing errors), they'd have the same node ID. The previous derivation method (XOR folding) is marginally more collision-resistant.

> [!NOTE]
> Low risk. The current approach is simpler and sufficient for small deployments.

---

### 14. `WedsGatewayApi` — no CORS headers on API responses

**File:** [WedsGatewayApi.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_gateway_api/WedsGatewayApi.cpp)

The API doesn't send `Access-Control-Allow-Origin` headers. If the dashboard is ever served from a different origin (e.g., during development with a local dev server), API calls will be blocked by CORS.

Currently the dashboard is served from the same ESP32, so same-origin policy is satisfied. This is a future concern only.

> [!NOTE]
> Consider adding CORS headers to `sendJson()` for development flexibility.

---

### 15. DataCollection `vTaskReadMQ2` — uses undefined variable `csvBuffer`

**File:** [DataCollection/src/logger/main.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/../DataCollection/src/logger/main.cpp) (~line 176)

```cpp
void vTaskReadMQ2(void *pvParameters) {
    // ...
    for (;;) {
        int analogValue = analogRead(MQ2_PIN);
        // ...
        File file = LittleFS.open("/dati_mq2.csv", FILE_APPEND);
        if (file) {
            file.println(csvBuffer);  // ← csvBuffer is not defined in this scope!
```

`csvBuffer` is a local variable in `vTaskDataLogger`, not in `vTaskReadMQ2`. This will fail to compile (or use a random stack value if somehow linked).

> [!WARNING]
> This is a compilation error in the DataCollection logger. The MQ2 task also has no `vTaskDelayUntil` call (it's commented out), so it would run in a tight loop consuming 100% CPU on its core.

**Fix:**
```diff
  void vTaskReadMQ2(void *pvParameters) {
+     char csvBuffer[128];
      TickType_t xLastWakeTime = xTaskGetTickCount();
      // ...
      for (;;) {
+         vTaskDelayUntil(&xLastWakeTime, xFrequency);  // uncomment this!
          int analogValue = analogRead(MQ2_PIN);
+         snprintf(csvBuffer, sizeof(csvBuffer), "%lu,MQ2,0,0,0,%d",
+                  millis(), analogValue);
```

---

### 16. DataCollection `logger_bme_only/main.cpp` — BSEC subscription count mismatch

**File:** [DataCollection/src/logger_bme_only/main.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/DataCollection/src/logger_bme_only/main.cpp#L49-L58)

```cpp
bsec_virtual_sensor_t sensorList[] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_STABILIZATION_STATUS,    // ← 7 items
};
iaqSensor.updateSubscription(sensorList, 6, BSEC_SAMPLE_RATE_LP);  // ← count says 6
```

The array has **7 elements** but `updateSubscription` is told there are **6**. `BSEC_OUTPUT_STABILIZATION_STATUS` won't be subscribed.

Additionally, the `file.printf` format string has **11 format specifiers** but the CSV header only has **8 columns**, so the output CSV will have misaligned columns.

**Fix:**
```diff
- iaqSensor.updateSubscription(sensorList, 6, BSEC_SAMPLE_RATE_LP);
+ iaqSensor.updateSubscription(sensorList, 7, BSEC_SAMPLE_RATE_LP);
```
And fix the CSV header to match the printf columns.

---

### 17. `WedsNodeComm::sendAck()` consumes a sequence ID — may confuse gateway

**File:** [WedsNodeComm.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_node_comm/WedsNodeComm.cpp)

```cpp
bool WedsNodeComm::sendAck(...) {
    const uint16_t ack_sequence_id = sequence_id_++;  // ← consumes a seq ID
    // ...
}
```

ACK packets consume a `sequence_id_` from the node's counter. This means the next `sendStatus()` or `sendAlert()` will skip a sequence number. The gateway tracks `last_sequence_id` per node — a gap in the sequence won't cause functional issues, but it means the sequence numbers aren't contiguous, which could confuse debugging.

> [!NOTE]
> Minor. Consider using a separate counter for ACK sequence IDs, or document that gaps in status sequence numbers are expected after alert mode commands.

---

## Summary Priority Table

| # | Severity | Issue | File |
|---|----------|-------|------|
| 2 | 🔴 Critical | Gateway registry accessed without mutex in `loop()` | [gateway/main.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/src/gateway/main.cpp) |
| 3 | 🔴 Critical | BSEC `nextCall` in nanoseconds, not milliseconds — infinite block | [WedsSensors.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_sensors/WedsSensors.cpp) |
| 4 | 🔴 Critical | Zeroed BSEC sample triggers cascading false alerts | [WedsSensors.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_sensors/WedsSensors.cpp) |
| 5 | 🔴 Critical | Simulation pressure ~0.6 instead of ~95000 Pa — risk model garbage | [WedsNodeSimulation.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_node_simulation/WedsNodeSimulation.cpp) |
| 6 | 🟠 High | Node timestamp always fake — risk model day features degraded | [node/main.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/src/node/main.cpp) |
| 7 | 🟠 High | 8KB stack may be insufficient for TFLite inference | [WedsNodeConfig.h](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/include/WedsNodeConfig.h) |
| 8 | 🟠 High | Legacy sensor NaN propagates through EMA/model permanently | [WedsSensors.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_sensors/WedsSensors.cpp) |
| 10 | 🟡 Medium | No retry limit/backoff for gateway alert commands | [WedsGatewayComm.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_gateway_comm/WedsGatewayComm.cpp) |
| 11 | 🟡 Medium | Battery read delay+pinMode under mutex | [WedsNodeState.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_node_state/WedsNodeState.cpp) |
| 15 | 🟡 Medium | DataCollection MQ2 task: undefined `csvBuffer`, no delay | [DataCollection logger](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/DataCollection/src/logger/main.cpp) |
| 16 | 🟡 Medium | BSEC subscription count mismatch (7 items, says 6) | [DataCollection logger_bme](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/DataCollection/src/logger_bme_only/main.cpp) |
| 1 | 🟢 Low | millis() wrap comment needed (signed-sub is actually safe) | [WedsNodeState.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_node_state/WedsNodeState.cpp) |
| 9 | 🟢 Low | `selfTestIndexForNode` returns 0 on failure | [gateway_self_test/main.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/src/gateway_self_test/main.cpp) |
| 13 | 🟢 Low | Node ID derivation simplified (lower 32 bits of MAC) | [WedsProtocol.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_protocol/WedsProtocol.cpp) |
| 14 | 🟢 Low | No CORS headers on API (same-origin OK for now) | [WedsGatewayApi.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_gateway_api/WedsGatewayApi.cpp) |
| 17 | 🟢 Low | ACK consumes status sequence ID (non-contiguous seq nums) | [WedsNodeComm.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_node_comm/WedsNodeComm.cpp) |

> [!TIP]
> **Immediate action items:**
> 1. Fix **#3** (BSEC nextCall units) — this blocks the node entirely with real sensors
> 2. Fix **#2** (gateway mutex race) — one-line change, prevents data corruption
> 3. Fix **#4** (zeroed sample guard) — prevents false alert cascades
> 4. Fix **#5** (simulation pressure) — enables meaningful testing in simulation mode
