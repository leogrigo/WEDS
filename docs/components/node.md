# Sensor Node Internals

Located inside `code/src/node/main.cpp`.

This component operates strictly by utilizing **FreeRTOS** Tasks. Because IoT microcontrollers are inherently single-threaded hardware, splitting responsibilities into Queue-locked Tasks allows concurrent sampling separated cleanly from heavy calculations.

## Main Threads
1. **`TaskSampleSensors`**: 
   - Periodically samples the real-world (or the simulation environment).
   - Packs data into an internal struct `sensors_sample_t`.
   - Populates an RTOS Queue `sens_result` and flips a notification bit `SENSOR_SAMPLED_BIT`.
2. **`TaskAnomalyDetection`**: 
   - Remains dormant until `SENSOR_SAMPLED_BIT` indicates fresh data.
   - Computes statistical deviations over time utilizing historical baselines.
   - Pushes an `anomaly_result_t` structure containing risk scores into `anomaly_queue`.
3. **`TaskRiskDetection`**: 
   - Parallel evaluation evaluating harsh non-statistical thresholds (e.g. static physical limits like Gas < 20000.0f).
4. **`TaskStateMachine`**: 
   - Blocks waiting for data streams from Anomaly queues.
   - Manages a **Warning Streak Counter**: It requires sequential consecutive infractions from the anomaly scores to upgrade from `WARNING` to `ALERT`. This efficiently debounces noisy environmental occurrences that are safe.
   - If an `ALERT` triggers, the state machine leverages the internal `LoraComm` engine pushing the data node update sequence over SPI.
