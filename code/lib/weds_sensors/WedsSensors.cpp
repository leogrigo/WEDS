#include "WedsSensors.h"

#include <Arduino.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>

namespace {

Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;

void printSample(const WedsSensorSample& sample) {
    Serial.printf(
        "[SENSOR] temp=%.2f C hum=%.2f %% pressure=%.2f Pa gas_raw=%.0f gas=%.1f %%\n",
        sample.temperature,
        sample.humidity,
        sample.pressure,
        sample.gas_resistance,
        sample.gas_resistance * 100.0f / 4095.0f
    );
}

}  // namespace

void weds_sensors_begin() {
    if (!Wire.begin(WEDS_I2C_SDA, WEDS_I2C_SCL)) {
        Serial.println("[SENSOR] I2C init failed");
    }

    pinMode(WEDS_MQ2_PIN, INPUT);

    if (!aht.begin()) {
        Serial.println("[SENSOR] AHT20 init failed");
    }

    if (!bmp.begin()) {
        Serial.println("[SENSOR] BMP280 init failed");
    }
}

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

    printSample(sample);
    return sample;
}
