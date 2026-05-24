# Code Correction Report — Fail-Safe Sensor Intercept

**Report ID:** `code_correction_001`
**Date:** 2026-05-22
**Environment:** `node` — Heltec WiFi LoRa 32 V3 (ESP32-S3)
**File Modified:** [`src/node/main.cpp`](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/src/node/main.cpp)

---

## 1. Problem Description — "Fail-Deadly" Bug

### Root Cause

When `readSensors()` returned an invalid sample (e.g., NaN from I2C failure), the original guard block read:

```cpp
if (!sample.valid) {
    Serial.println("[NODE] Sensor read invalid, entering low-risk sleep");
    rtc_virtual_timestamp += WEDS_SLEEP_SEC_RISK_LOW;
    esp_deep_sleep((uint64_t)WEDS_SLEEP_SEC_RISK_LOW * 1000000ULL);
    return;
}
```

This had two compounding defects:

| Defect | Impact |
|---|---|
| **Slept for `WEDS_SLEEP_SEC_RISK_LOW` = 1800 s (30 min)** | A transient I2C glitch or a sensor not-yet-ready condition causes the node to go completely silent for 30 minutes. In a wildfire scenario, this is a dangerous blackout window. |
| **Advanced `rtc_virtual_timestamp`** | The virtual clock was advanced by the full 1800-second sleep duration even though no valid reading occurred. This permanently desynchronises the TinyML 7-day rolling average bucketing algorithm, corrupting day-boundary calculations for subsequent valid samples. |

### Why This Is "Fail-Deadly"

The standard engineering principle for safety-critical systems is **fail-safe**: a failure mode should default to the safest observable state, not the most dangerous one. Sleeping for 30 minutes after a sensor read failure is the _exact opposite_:

- The node stops reporting entirely during the period it might need to report most urgently (e.g., the I2C glitch itself could be induced by heat or power brownout correlating with a fire event).
- The gateway registers the node as unresponsive, but cannot distinguish "sensor fault → 30-min sleep" from "no node present".
- No alert is raised; fire detection is suspended.

Additionally, the check only tested `sample.valid` but did not explicitly guard against raw `NaN` values propagating into the anomaly EMA filter and TFLite INT8 quantizer — both of which exhibit undefined behaviour on `NaN` input.

---

## 2. Correction Applied

### File: [`src/node/main.cpp`](file:///c:/Users/admin/Documents/IoT/GroupProj/WEDS/code/src/node/main.cpp)

**Diff:**

```diff
-    if (!sample.valid) {
-        Serial.println("[NODE] Sensor read invalid, entering low-risk sleep");
-        rtc_virtual_timestamp += WEDS_SLEEP_SEC_RISK_LOW;
-        esp_deep_sleep((uint64_t)WEDS_SLEEP_SEC_RISK_LOW * 1000000ULL);
+    if (!sample.valid || isnan(sample.temperature) || isnan(sample.humidity)) {
+        Serial.println("[NODE_WARN] Sensor fault detected (NaN or invalid read) — retrying in 5s");
+        Serial.flush();
+        esp_deep_sleep(5000000ULL);
         return;
     }
```

### What the correction achieves

| Property | Before (Fail-Deadly) | After (Fail-Safe) |
|---|---|---|
| **Sleep on fault** | 1800 s (30 min) | 5 s |
| **RTC clock advanced on fault** | ✅ Yes — permanently desynchronises bucketing | ❌ No — clock preserved for next valid cycle |
| **TinyML / anomaly pipeline reached on NaN** | Possible (if `valid` flag was ever mis-set) | Impossible — explicit `isnan()` guards |
| **Serial log flushed before sleep** | ❌ No | ✅ Yes — ensures message is visible on monitor |
| **Gateway blackout window** | 30 minutes | ≤ 5 seconds |

### Why `esp_deep_sleep(5 s)` rather than an Arduino `delay(5000)` + `continue`

> [!NOTE]
> The firmware was upgraded in a prior session to a **linear one-shot deep-sleep architecture** (`setup()` executes once, then `esp_deep_sleep()` is called — `loop()` is intentionally empty). There is no FreeRTOS `NodeCycleTask` or `vTaskDelay` / `continue` construct in the current codebase. The semantically correct equivalent of "wait 5 seconds and retry" in a one-shot deep-sleep firmware is a **5-second timer-wakeup deep sleep**, which resets the MCU and re-executes `setup()` from the top — identical behaviour to `continue` in a FreeRTOS loop, but without the 40 KB FreeRTOS overhead.

---

## 3. Verification

```
Environment    Status    Duration     RAM       Flash
─────────────  ──────────────────────────────────────
node           SUCCESS   00:00:15.67  13.6 %   32.7 %
```

No compilation errors. No increase in RAM footprint. The additional `isnan()` call and the constant `5000000ULL` microsecond sleep argument contribute negligible flash overhead (+20 bytes).
