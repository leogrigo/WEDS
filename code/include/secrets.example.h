#pragma once

#include <stdint.h>

/** @brief WiFi SSID for the gateway connection. */
constexpr char secret_wifi[] = "YOUR_WIFI_SSID";

/** @brief WiFi password for the gateway connection. */
constexpr char secret_password[] = "YOUR_WIFI_PASSWORD";

/** @brief Local MQTT broker hostname or IP address. */
constexpr char secret_mqtt_host[] = "YOUR_MQTT_HOST";

/** @brief Local MQTT broker port. */
constexpr uint16_t secret_mqtt_port = 1883;
