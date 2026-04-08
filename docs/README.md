# WEDS - Wildfire Early Detection System

Welcome to the **Wildfire Early Detection System (WEDS)** codebase.

WEDS is an IoT Proof of Concept built using ESP32 chips equipped with LoRa modules (specifically the *Heltec WiFi LoRa 32 V3* board). Its goal is to provide deeply embedded, remote environmental sensing specifically tailored to identifying rapid wildfire occurrences through a customized machine anomaly filter preventing false alarms.

## Repository Map

The codebase resides in `code/` utilizing the **PlatformIO** ecosystem format. The project uses standard Arduino wrappers around Espressif SDK and FreeRTOS for robust multi-tasking.

- `code/src/node/` - Contains the firmware for the remote Sensor Node.
- `code/src/gateway/` - Contains the firmware for the web-connected Gateway endpoint.
- `code/lib/` - Shared libraries abstracted for distinct separation of concerns.
- `docs/` - System architecture and component documentation.

## Documentation Index

Explore the following markdown files to understand how WEDS is built and why decisions were made:

- [Architecture Overview](architecture.md) - How the nodes talk to gateways over LoRa.
- [Sensor Node Internals](components/node.md) - Understanding the RTOS queues and sensing limits.
- [Gateway Internals](components/gateway.md) - Understanding the persistent polling and Web Server logic.
- [Shared Libraries](components/libraries.md) - Understanding the Z-Score math, custom EMA filters, and simulation orchestrators.
