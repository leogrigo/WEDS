# WEDS Architecture Overview 

The WEDS IoT system relies on two main components talking wirelessly over **LoRa** (using the SX1262 Chip).

## 1. The Sensor Node (Sender)
Deployed remotely in potentially vulnerable forests, this node is solely tasked with extracting physical metrics, judging if they pose a threat locally, and pushing telemetry up the pipeline ONLY when significant anomaly events orchestrate it, preserving bandwidth and energy.

## 2. The Gateway (Receiver and Web API Layer)
Placed closer to network centers or rangers' towers, it maintains a continuous web-connected socket while actively scanning the LoRa frequency for distressed Sensor Nodes messages.

## Data Flow Pipeline

1. **Environment Simulation**: Instead of real I2C sensors during development, the physical values (Temperature, Pressure, Humidity, Gas) are mocked over strict cyclic logic with random noise. 
2. **Anomaly Engine**: Raw signals go through an Exponential Moving Average (EMA) filter, isolating the baseline from sharp variances. Fast variances mapping precisely to Wildfire definitions (fast gas drop, heat acceleration) trigger positive standard deviations.
3. **Risk Enforcement**: Short momentary peaks are ignored. If consecutive risk events bypass standard deviations triggers a threshold, the state machine enters an `ANOMALY_ALERT`.
4. **Wireless TX**: The status payload is packed efficiently into a CSV string (`id=...,state=...,temp=...`) and pulsed out via LoRa.
5. **Gateway Intake**: The listening ESP32 caches the raw transmission string and evaluates RSSI and SNR constraints.
6. **Web Hydration**: A Web developer polls `http://<IP>/node_status` cleanly scraping the JSON-translated representation of that exact status payload for front-end dashboards!
