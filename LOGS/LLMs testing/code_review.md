# WEDS Firmware — Code Review & Validation Report

This report presents a comprehensive validation, testing, and correction analysis of the WEDS (Wildfire Early Detection System) firmware covering the **node**, **gateway**, and **gateway_self_test** firmware environments, as well as the shared internal libraries.

---

## 🛠️ Compilation & Environment Validation

All three target PlatformIO environments were tested and verified using surgical compilation checks:

| Environment | Platform | Board | Compilation Status | RAM Usage | Flash Usage |
|---|---|---|---|---|---|
| **`node`** | `espressif32` | `heltec_wifi_lora_32_V3` | **SUCCESS** ✅ | 13.6% (44,476 B) | 32.7% (1,093,485 B) |
| **`gateway`** | `espressif32` | `heltec_wifi_lora_32_V3` | **SUCCESS** ✅ | 54.3% (177,864 B) | 27.7% (927,133 B) |
| **`gateway_self_test`** | `espressif32` | `heltec_wifi_lora_32_V3` | **SUCCESS** ✅ | 53.8% (176,192 B) | 25.6% (855,841 B) |

---

## 🐞 Analysis & Verification of Critical Bugs

An exhaustive review of the codebase was conducted to identify critical architectural bugs, data races, timing discrepancies, and numerical bugs.

### 1. Gateway Radio Task Data Race (#2)
* **File:** [gateway/main.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/src/gateway/main.cpp#L57-L68)
* **Problem:** In the original design, `gatewayComm.loop()` received packets and updated the node registry via `registry_->ingestNodeStatus()` without acquiring `registryMutex`. This created a direct data race against `GatewayApiTask` which serializes registry entries to JSON.
* **Verification:** The current codebase **correctly holds `lockRegistry()`** across both `gatewayComm.loop()` and `gatewayComm.poll()`, eliminating the data race:
  ```cpp
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
  ```
  **Status:** Fixed & Validated. ✅

### 2. BSEC nextCall Time Unit Mismatch (#3)
* **File:** [WedsSensors.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_sensors/WedsSensors.cpp#L151-L154)
* **Problem:** BSEC's internal `nextCall` variable is defined in nanoseconds since boot. Comparing it directly to `millis()` (in milliseconds) caused astronomical delay durations, resulting in an indefinite node freeze after the first sample.
* **Verification:** The current codebase **correctly scales nanoseconds to milliseconds** before returning:
  ```cpp
  int64_t weds_sensor_next_call_ms(){
      if(!checkBmeSensorStatus()) return -1;
      return bmeSensor.nextCall / int64_t(1000000); // Convert ns -> ms
  }
  ```
  **Status:** Fixed & Validated. ✅

### 3. Simulation Mode Pressure Mismatch (#5)
* **File:** [WedsNodeSimulation.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_node_simulation/WedsNodeSimulation.cpp#L49)
* **Problem:** Simulation generated `pressure = 0.60f + sampleNormal(0.0f, 0.02f)`, representing fractional kilo-Pascals. The TinyML risk model expects raw Pascals (~95000 Pa). This mismatch generated wildly out-of-distribution pressure features, producing garbage risk outputs during simulation testing.
* **Verification:** The simulator was verified to **correctly generate Pascals**:
  ```cpp
  float pressure = 95000.0f + sampleNormal(0.0f, 500.0f); // Pa
  ```
  **Status:** Fixed & Validated. ✅

---

## 🔧 Surgical Corrections Performed

### 1. Legacy Sensor NaN Propagation Prevention (#8)
* **File:** [WedsSensors.cpp](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/lib/weds_sensors/WedsSensors.cpp#L48-L66)
* **Severity:** 🟠 High
* **Issue:** Legacy sensor reads from AHT20 and BMP280 return `NaN` upon I2C bus faults, power glitches, or loose cabling. Without checking, `NaN` values propagate through the EMA baseline filters in `WedsAnomalyDetector` (corrupting it permanently) and the TinyML model's INT8 quantizers, rendering the node permanently broken until a hard reboot.
* **Surgical Fix:** Added `isnan` checks on the environmental variables (`temperature`, `humidity`, `pressure`) to safely mark the sample invalid:
  ```cpp
  WedsSensorSample weds_read_environment_sample() {
      sensors_event_t humidity;
      sensors_event_t temp;

      const float bmp_temperature = bmp.readTemperature();
      const float pressure = bmp.readPressure();
      aht.getEvent(&humidity, &temp);

      WedsSensorSample sample{};
      sample.temperature = bmp_temperature;
      sample.humidity = humidity.relative_humidity;
      sample.pressure = pressure;
      sample.gas_resistance = static_cast<float>(analogRead(WEDS_MQ2_PIN));
      
      if (isnan(sample.temperature) || isnan(sample.humidity) || isnan(sample.pressure)) {
          sample.valid = false;
          Serial.println("[SENSOR] Error: read NaN from environmental sensors");
      } else {
          sample.valid = true;
      }

      printSample(sample);
      return sample;
  }
  ```
  With this validation, `main.cpp` detects the `!sample.valid` state and handles it gracefully by bypassing ML/anomaly pipelines and sleeping.
* **Status:** Corrected & Verified via compiler check. ✅

---

## 📋 Comprehensive Vulnerability Reference

| Issue | Severity | Description | Status |
|---|---|---|---|
| **1** | 🟢 Low | `millis()` wrap in `alert_mode_until_ms_` uses signed comparison tricks that prevent failure up to 24.8 days. | Informational (Safe) |
| **2** | 🔴 Critical | Gateway registry accessed without mutex inside radio loop task. | Fixed (Validated) |
| **3** | 🔴 Critical | BSEC `nextCall` unit mismatch (nanoseconds vs. milliseconds) blocks node indefinitely. | Fixed (Validated) |
| **4** | 🔴 Critical | Zeroed sample from unready BSEC sensor causes false alarms. | Fixed (Bypassed via `.valid`) |
| **5** | 🔴 Critical | Simulation pressure is configured as ~0.6 instead of ~95,000 Pa, feeding corrupted features to TFLite. | Fixed (Validated) |
| **6** | 🟠 High | Fake/constant node timestamp degrades rolling multi-day average accuracy. | Mitigated (RTC Virtual Clock added) |
| **7** | 🟠 High | 8KB FreeRTOS stack tight for TFLite Micro operator scratchpads. | Mitigated (One-shot linear deep sleep) |
| **8** | 🟠 High | Legacy sensor read failure propagates NaNs permanently into the EMA anomaly baseline. | **Surgically Fixed** ✅ |
| **9** | 🟢 Low | `selfTestIndexForNode` defaults to index 0 on failure instead of an error state. | Informational (Static checks OK) |
| **10** | 🟡 Medium | No exponential backoff or retry limit for failed LoRa alert commands from the gateway. | Future Optimization |
| **11** | 🟡 Medium | Heltec battery ADC reconfigures GPIO pin mode and delays 5ms while locking the registry mutex. | Future Optimization |
| **12** | 🟡 Medium | MQ2 simulation values lack dynamic alignment with physical sensors. | Informational |
| **13** | 🟢 Low | XOR MAC fold truncation simplified to lower 32-bits of MAC for unique Node ID. | Informational (Acceptable) |
| **14** | 🟢 Low | Lack of CORS headers on the web API (not a problem since dashboard is same-origin). | Informational |
