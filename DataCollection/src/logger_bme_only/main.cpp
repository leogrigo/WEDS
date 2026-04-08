#include <Arduino.h>
#include <Wire.h>
#include <LittleFS.h>
#include <bsec.h>

// --- PINOUT ---
#define I2C_SDA 41
#define I2C_SCL 42

// --- ISTANZA SENSORE ---
Bsec iaqSensor;
unsigned long uptimeSeconds = 0;
int nextSequenceTimeMs = 0;
// --- PROTOTIPI ---
void checkIaqSensorStatus();

void setup() {
  Serial.begin(115200);
  delay(500);

  // Inizializza I2C e FileSystem
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!LittleFS.begin(true)) {
    Serial.println("Errore LittleFS!");
    return;
  }

  // Crea intestazione CSV se il file non esiste
  if (!LittleFS.exists("/dati_bme680.csv")) {
    File file = LittleFS.open("/dati_bme680.csv", FILE_WRITE);
    if (file) {
      file.println("Uptime(s),IAQ,IAQ Accuracy,Temp(C),Humidity(%),Pressure(hPa),CO2_eq(ppm),bVOC_eq(ppm)");
      file.close();
    }
  }
  
  // 1. Inizializzazione BSEC
  iaqSensor.begin(BME68X_I2C_ADDR_HIGH, Wire);
  checkIaqSensorStatus();

  // 2. Iscrizione ai canali BSEC in modalità LP (Low Power - 1 sample ogni 3s)
  bsec_virtual_sensor_t sensorList[] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_STABILIZATION_STATUS,
    
  };
  iaqSensor.updateSubscription(sensorList, 6, BSEC_SAMPLE_RATE_LP);

  Serial.println("Inizio letture ogni 3 secondi. Attendere calibrazione...");
}

void loop() {
  // .run() usa automaticamente millis() e restituisce 'true' ESATTAMENTE ogni 3 secondi
  if (iaqSensor.run()) {
    
    uptimeSeconds = millis() / 1000;

    // Scrive nel CSV
    File file = LittleFS.open("/dati_bme680.csv", FILE_APPEND);
    if (file) {
      file.printf("%lu,%.2f,%d,%.2f,%.2f,%.2f,%.2f,%d,%.2f,%d,%.2f\n", 
                  uptimeSeconds,
                  iaqSensor.iaq,
                  iaqSensor.iaqAccuracy,
                  iaqSensor.temperature,
                  iaqSensor.humidity,
                  iaqSensor.pressure / 100.0,
                  iaqSensor.co2Equivalent,
                  iaqSensor.co2Accuracy,
                  iaqSensor.breathVocEquivalent,
                  iaqSensor.breathVocAccuracy,
                  iaqSensor.stabStatus);
      file.close();
      
      // Stampa su Monitor Seriale
      Serial.printf("Uptime: %lus | IAQ: %.1f (Acc:%d) | T: %.1fC | H: %.1f%% | CO2: %.1f (Acc:%d) | bVOC: %.1f (Acc:%d)| Stab: %.1f\n", 
                    uptimeSeconds, iaqSensor.iaq, iaqSensor.iaqAccuracy, iaqSensor.temperature, iaqSensor.humidity, iaqSensor.co2Equivalent, iaqSensor.co2Accuracy, iaqSensor.breathVocEquivalent, iaqSensor.breathVocAccuracy, iaqSensor.stabStatus);
    } else {
      Serial.println("Errore apertura CSV!");
    }
  }

  // Qui checkiamo lo stato nel caso ci siano errori a runtime
  checkIaqSensorStatus();
}

// --- FUNZIONI DI SUPPORTO BSEC ---
void checkIaqSensorStatus() {
  if (iaqSensor.bsecStatus != BSEC_OK) {
    if (iaqSensor.bsecStatus < BSEC_OK) {
      Serial.printf("ERRORE CRITICO BSEC: %d\n", iaqSensor.bsecStatus);
    } else {
      Serial.printf("WARNING BSEC: %d\n", iaqSensor.bsecStatus);
    }
  }
}