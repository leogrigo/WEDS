#include "WedsSensors.h"

#include <Arduino.h>

#ifdef WEDS_LEGACY_SENSORS

#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>

namespace {

Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;

/**
 * @brief Prints a sample from the legacy sensors to the serial console.
 * @param sample The sensor sample to print.
 */
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
    sample.valid = true;

    printSample(sample);
    return sample;
}

#else

#include <bsec.h>
#include <Wire.h>

#define I2C_ADDRESS BME68X_I2C_ADDR_HIGH
#define BSEC_SENSOR_RATE BSEC_SAMPLE_RATE_LP

Bsec bmeSensor;


uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
int64_t rtc_simulated_millis = 0;
uint64_t read_count = 0;

/**
 * @brief Validates the status of the BME68X sensor and BSEC library.
 * @return true if the sensor is functioning correctly, false otherwise.
 */
bool checkBmeSensorStatus(void) {
  if (bmeSensor.bsecStatus != BSEC_OK || bmeSensor.bme68xStatus != BME68X_OK) {
    Serial.println("ERRORE SENSORE - Controllo cablaggio e I2C!");
    Serial.println("Codice bme68xStatus: " + String(bmeSensor.bme68xStatus));
    Serial.println("Codice bsecStatus: " + String(bmeSensor.bsecStatus));
    Serial.flush();
    return false;
  }
  return true;
}

bool weds_sensors_begin(){
    if(!Wire1.begin(WEDS_I2C_SDA, WEDS_I2C_SCL)){
        Serial.println("[SENSOR] I2C init failed");
        return false;
    }

    bmeSensor.begin(I2C_ADDRESS, Wire1);

    bsec_virtual_sensor_t sensorList[7] = {
        BSEC_OUTPUT_RAW_PRESSURE,
        BSEC_OUTPUT_RAW_GAS,
        BSEC_OUTPUT_STATIC_IAQ,
        BSEC_OUTPUT_CO2_EQUIVALENT,
        BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    };

    bmeSensor.updateSubscription(sensorList, 7, BSEC_SENSOR_RATE);

    return checkBmeSensorStatus();

}

bool weds_sensor_save_state(){
    bmeSensor.getState(bsecState);
    
    return checkBmeSensorStatus();
}

bool weds_sensor_load_state(uint8_t *state){
    if(state == nullptr || state == NULL){
        
        bmeSensor.setState(bsecState);
    }
    else{
        memcpy(bsecState, state, BSEC_MAX_STATE_BLOB_SIZE);
        bmeSensor.setState(bsecState);
    }
    return checkBmeSensorStatus();
}

WedsSensorSample weds_read_environment_sample(){
    WedsSensorSample sample{};
    if(bmeSensor.run()) {
        sample.temperature = bmeSensor.temperature;
        sample.humidity = bmeSensor.humidity;
        sample.pressure = bmeSensor.pressure;
        sample.gas_resistance = bmeSensor.gasResistance;
        sample.valid = true;
    }
    return sample;
}

int64_t weds_sensor_next_call_ms(){
    if(!checkBmeSensorStatus()) return -1;
    return bmeSensor.nextCall / int64_t(1000000);
}


#endif