# Gateway Node Internals

Located inside `code/src/gateway/main.cpp`.

This node serves as the translation bridge between typical Internet constraints (TCP/JSON) and Deep-Field embedded signals (LoRa/SPI).

## Core Systems

### 1. Web API Layer
Utilizing `<WebServer.h>`, the node connects to a localized `WiFi` network explicitly using secrets imported locally.
- Provides a clean `/` endpoint confirming the machine is up.
- Exposes a robust `/node_status` endpoint providing an HTTP representation of the latest known status parsed as JSON. It abstracts escaping, quotation marks formatting directly in native C++.

### 2. Event Polling
Because Gateway needs to ensure minimum packet loss natively, it dedicates the infinite CPU `loop()` function to constant LoRa interrogations via `LoraGateway::pollReceive()`.

When a packet is discovered natively by an interrupt (`packetReceived` boolean), it leverages the SPI buffer. It extracts the raw text and queries physical Radio parameters identifying if the sender is near or far utilizing:
- `rssi` (Received Signal Strength Indicator)
- `snr` (Signal-to-Noise Ratio)

Once transformed utilizing `payloadToJson`, it mutates a localized `lastNodeStatus` cache. The next web request hits immediately get that latest iteration in less than ~15 milliseconds.
