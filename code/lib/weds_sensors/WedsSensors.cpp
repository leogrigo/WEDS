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
    
    if (isnan(sample.temperature) || isnan(sample.humidity) || isnan(sample.pressure)) {
        sample.valid = false;
        Serial.println("[SENSOR] Error: read NaN from environmental sensors");
    } else {
        sample.valid = true;
    }

    printSample(sample);
    return sample;
}

#else

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

// BME68X_I2C_ADDR_HIGH corrisponde solitamente a 0x77
#define I2C_ADDRESS 0x77 

// Istanziamo il BME680 agganciandolo al bus I2C specificato da WEDS_I2C_SCL/SDA
Adafruit_BME680 bmeSensor(&Wire1); 

/**
 * @brief Funzione fittizia per retrocompatibilità. Senza BSEC non ci sono stati d'errore complessi.
 */
bool checkBmeSensorStatus(void) {
  return true;
}

bool weds_sensors_begin(){
    if(!Wire1.begin(WEDS_I2C_SDA, WEDS_I2C_SCL)){
        Serial.println("[SENSOR] I2C init failed");
        return false;
    }

    // Inizializza il sensore
    if (!bmeSensor.begin(I2C_ADDRESS)) {
        Serial.println("ERRORE SENSORE - Impossibile trovare BME680 su I2C!");
        Serial.flush();
        return false;
    }

    // Configurazione dei parametri hardware per la lettura di Adafruit
    bmeSensor.setTemperatureOversampling(BME680_OS_8X);
    bmeSensor.setHumidityOversampling(BME680_OS_2X);
    bmeSensor.setPressureOversampling(BME680_OS_4X);
    bmeSensor.setIIRFilterSize(BME680_FILTER_SIZE_3);
    
    // Imposta la piastra termica a 320°C per 150 millisecondi (profilo a basso consumo)
    bmeSensor.setGasHeater(320, 150);

    return true;
}

bool weds_sensor_save_state(){
    // La libreria Adafruit lavora in Forced Mode stateless:
    // Nessuno stato interno (come l'IAQ di BSEC) da salvare in Flash/RTC.
    return true;
}

bool weds_sensor_load_state(uint8_t *state){
    // Nessuno stato da caricare al risveglio.
    return true;
}

WedsSensorSample weds_read_environment_sample(){
    WedsSensorSample sample{};
    
    // bmeSensor.performReading() "sveglia" il sensore, scalda il gas, preleva i dati e lo spegne
    if(bmeSensor.performReading()) {
        sample.temperature = bmeSensor.temperature;
        sample.humidity = bmeSensor.humidity;
        sample.pressure = bmeSensor.pressure;
        sample.gas_resistance = bmeSensor.gas_resistance;
        sample.valid = true;
    } else {
        sample.valid = false;
        Serial.println("[SENSOR] Error: Lettura dal BME680 fallita!");
    }
    
    return sample;
}

int64_t weds_sensor_next_call_ms(){
    // Con Adafruit decidi tu quando chiamare il sensore (es. ogni 10 minuti in deep sleep).
    // Restituiamo 0 in modo da informare il sistema che non ci sono vincoli di attesa.
    return 0;
}

#endif

void weds_print_sample(const WedsSensorSample& sample) {
    Serial.printf(
        "[SENSOR] timestamp=%lu temp=%.2f C hum=%.2f %% pressure=%.2f Pa gas=%.2f valid=%s\n",
        (unsigned long)sample.timestamp,
        sample.temperature,
        sample.humidity,
        sample.pressure,
        sample.gas_resistance,
        sample.valid ? "yes" : "no"
    );
}