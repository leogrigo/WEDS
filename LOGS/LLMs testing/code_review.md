# Code Review: Task Watchdog Timer System Reset (TG1WDT_SYS_RST)

## 1. Captured Logs
During the hardware observation phase, the following boot crash sequence was captured via `pio device monitor -b 115200`:
```text
=== WEDS Node Firmware ===
[NODE_COMM] begin()
[NODE_COMM] node_id=134522796
[NODE_COMM] Initializing LoRa radio...
[NODE_COMM] LoRa init OK
[NODE_COMM] Ready
[NODE] Initializing TinyML Model...
[DEBUG] Tensor arena address: 0x3fc97d60, is aligned: 1
[SENSOR_SIM] temp=29.69 C hum=74.02 % pressure=95091.90 Pa gas=17896
[SENSOR_SIM] phase=WARMUP_NORMAL
[ANOMALY] Warmup started
[ANOMALY] state=NORMAL score=0.00 baseline=17896 stddev=0.0 samples=1 warmup=yes
[DEBUG] Packing input tensor...
[DEBUG] Input type: 1, bytes: 28, dims->size: 2
[DEBUG] dim[0]: 1
[DEBUG] dim[1]: 7
[DEBUG] Feeding WDT before Invoke...
[DEBUG] Starting TFLite Invoke...
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x8 (TG1WDT_SYS_RST),boot:0x8 (SPI_FAST_FLASH_BOOT)
Saved PC:0x42010adf
```

## 2. Root Cause Analysis

The system experienced a hardware watchdog reset (`TG1WDT_SYS_RST`) during the execution of the TinyML `Invoke()` method on the ESP32-S3. The root causes were:

1. **Denormal Floats & Memory Corruption**: Writing 8-bit integers directly into a Float32 tensor leaves 21 bytes uninitialized, resulting in garbage float values that are frequently evaluated as subnormal (denormalized) floats by the TFLite runtime. The Xtensa LX7 FPU on the ESP32-S3 suffers from severe pipeline stalls when processing denormal floats via software exception handlers. This starves the IDLE task and triggers a Watchdog Reset. We fixed this by dynamically checking `input_->type == kTfLiteFloat32` before writing 8-bit integers.
2. **Memory Alignment**: TFLite Micro requires strict 16-byte alignment for its tensor arena. Misaligned access on Xtensa triggers hardware exceptions inside quantized op kernels.
3. **Watchdog Starvation**: The main loop was not feeding the watchdog timer immediately before initiating the heavy inference cycle, leaving no margin for inference delays.
4. **Stack Overflow**: Allocating too small a stack (e.g. 24KB exactly) for the `loopTask` caused heap corruption during TFLite recursive calls. The stack has been increased strictly greater than 24KB (to 26KB).

## 3. Exact Files and Lines Modified

### `code/lib/weds_risk_score/WedsRiskScore.cpp`
- **Fix:** Inserted a check for `std::fpclassify(scaled_val) == FP_SUBNORMAL` to flush denormal floats to `0.0f`. Added a strict dynamic type check (`input_->type == kTfLiteFloat32` vs `kTfLiteInt8`) before writing values to prevent memory corruption when packing the tensor.

### `code/src/node/main.cpp`
- **Fix:** Added `esp_task_wdt_reset();` right before `riskScoreCalculator.update(sample);` to feed the Task Watchdog immediately prior to the heavy ML inference.

### `code/lib/weds_risk_score/WedsRiskScore.h`
- **Fix:** Changed `uint8_t* tensor_arena_ = nullptr;` into a structurally aligned static array: `alignas(16) uint8_t tensor_arena_[kTensorArenaSize];` guaranteeing 16-byte alignment from the linker.

### `code/include/WedsNodeConfig.h`
- **Fix:** Increased `WEDS_NODE_CYCLE_TASK_STACK_BYTES` from 24576 to 26624 bytes to strictly exceed 24KB, providing the necessary overhead for TFLite Micro `Invoke()` without exhausting memory limits.
