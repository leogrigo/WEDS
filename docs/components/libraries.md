# Shared Domain Libraries

To cleanly separate code logic preventing nested overlapping inside PlatformIO, the components live discretely in `code/lib/`.

## 1. `lib/lora_node` & `lib/lora_gateway`
Because both endpoints have uniquely different hardware priorities:
- **Node LoRa:** Initializes SPI. Manages *Wake and Sleep* to aggressively preserve battery and only Tx (Transmit) the status string payload efficiently when the local inference machine dictates. 
- **Gateway LoRa:** Initializes SPI. Sits continuously in RX mode (Polling) mapped to hardware interrupts indicating `packetReceived`. Returns structured parsed structs indicating signal decay (`rssi` and `snr`).

## 2. `lib/utils`
Common underlying statistics block:
- **Exponential Moving Average Filter (EMA)**. Allows smooth blending of older points iteratively without holding large static arrays in RAM, mathematically filtering generic noise from important underlying sensor moves.
- **`sampleNormal` & `computeZScore`**: Traditional mathematical formulas mapping deviations inside physical scopes uniformly.

## 3. `lib/simulation`
WEDS allows developers to test the physical responses using mock setups without requiring massive smoke chambers to burn in laboratories. It accomplishes this via State Simulation Machine modes.
Available scenarios:
1. `SIM_MODE_NORMAL`: Standard cyclic sine waves matching normal day/night environments.
2. `SIM_MODE_DRY_PERIOD`: Slowly removes humidity increasing temperature indefinitely simulating severe dry spells.
3. `SIM_MODE_GAS_DROP`: Instantiates a drastic dip in ambient gas resistances indicating immediate presence of complex fumes. 
4. `SIM_MODE_FIRE_EVENT`: Synergistically blends heat explosion, extreme loss of moisture and volatile gases simulating a complete localized rapid burning incident.
5. `SIM_MODE_SENSOR_FAULT`: Simulates physical damage to the I2C wires randomly keeping sensors frozen.

## 4. `lib/anomalies`
Responsible for grading the incoming signals mathematically against what the EMA baseline states safely. It generates directional bounds predicting wildfire components specifically relying highly on the fact that Wildfires require *Temperature Rising* and *Humidity/Gas Falling* cohesively.
