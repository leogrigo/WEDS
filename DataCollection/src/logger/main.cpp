#include <Arduino.h>
#include <Wire.h>
#include <LittleFS.h>

// Librerie Sensori
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_BME680.h>
#include <DHT.h>

#define SAMPLING_FREQUENCY_MS 3000

// --- PINOUT (Modificali in base ai tuoi collegamenti) ---
#define I2C_SDA 21
#define I2C_SCL 22
#define DHT_PIN 5
#define DHT_TYPE DHT22
#define MQ2_PIN 27

// --- INDIRIZZI I2C (Verifica i tuoi moduli) ---
#define BMP280_ADDR 0x76 
#define BME680_ADDR 0x77 

// --- STRUTTURE E FREERTOS ---
struct SensorData {
  uint32_t timestamp;
  const char* sensorName;
  float temperature;
  float humidity;
  float pressure;
  float gas;
};

QueueHandle_t dataQueue;
SemaphoreHandle_t i2cMutex;

// --- ISTANZE SENSORI ---
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
Adafruit_BME680 bme;
DHT dht(DHT_PIN, DHT_TYPE);

// --- TASK PROTOTYPES ---
void vTaskReadI2CSensors(void *pvParameters);
void vTaskReadDHT22(void *pvParameters);
void vTaskReadMQ2(void *pvParameters);
void vTaskDataLogger(void *pvParameters);

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  // Inizializza LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("Errore nel mount di LittleFS!");
    return;
  }

  // Crea file CSV con Header se non esiste
  if (!LittleFS.exists("/dati_sensori.csv")) {
    File file = LittleFS.open("/dati_sensori.csv", FILE_WRITE);
    if (file) {
      file.println("Timestamp(ms),Sensor,Temp(C),Humidity(%),Pressure(hPa),Gas(Ohm/Analog)");
      file.close();
    }
  }

  // // Inizializza I2C
  // Wire.begin(I2C_SDA, I2C_SCL);

  // // Inizializza Sensori
  // Serial.println("Inizializzazione sensori...");
  if (!aht.begin()) Serial.println("Errore AHT20!");
  if (!bmp.begin()) Serial.println("Errore BMP280!");
  // if (!bme.begin(BME680_ADDR)) Serial.println("Errore BME680!");
  // dht.begin();
  pinMode(MQ2_PIN, INPUT);

  // // Setta parametri ottimali BME680
  // bme.setTemperatureOversampling(BME680_OS_8X);
  // bme.setHumidityOversampling(BME680_OS_2X);
  // bme.setPressureOversampling(BME680_OS_4X);
  // bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  // bme.setGasHeater(320, 150); // 320*C per 150 ms

  // --- CREAZIONE OGGETTI FREERTOS ---
  dataQueue = xQueueCreate(20, sizeof(SensorData));
  i2cMutex = xSemaphoreCreateMutex();

  if (dataQueue == NULL || i2cMutex == NULL) {
    Serial.println("Errore creazione Queue o Mutex!");
    return;
  }

  // --- CREAZIONE TASKS ---
  // xTaskCreate(Funzione, Nome, Stack, Parametri, Priorità, Handle)
  xTaskCreate(vTaskReadI2CSensors, "TaskI2C", 8192, NULL, 3, NULL);
  // xTaskCreate(vTaskReadDHT22,      "TaskDHT", 4096, NULL, 3, NULL);
  xTaskCreate(vTaskReadMQ2,        "TaskMQ2", 4096, NULL, 3, NULL);
  xTaskCreate(vTaskDataLogger,     "TaskLog", 8192, NULL, 2, NULL); 
}

void loop() {
  // Il loop di Arduino rimane vuoto. FreeRTOS gestisce tutto nei task.
  vTaskDelete(NULL); 
}

// ------------------------------------------------------------------
// TASK 1: Lettura Sensori I2C (AHT20, BMP280, BME680)
// ------------------------------------------------------------------
void vTaskReadI2CSensors(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(SAMPLING_FREQUENCY_MS); // Legge ogni 5 secondi

  SensorData data;

  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      
      // Lettura AHT20
      sensors_event_t humidity, temp;
      aht.getEvent(&humidity, &temp);
      data = SensorData{millis(), "AHT20", temp.temperature, humidity.relative_humidity, 0, 0};
      xQueueSend(dataQueue, &data, portMAX_DELAY);

      // Lettura BMP280
      float bmpTemp = bmp.readTemperature();
      float bmpPress = bmp.readPressure() / 100.0F; // in hPa
      data = SensorData{millis(), "BMP280", bmpTemp, 0, bmpPress, 0};
      xQueueSend(dataQueue, &data, portMAX_DELAY);

      // // Lettura BME680
      // if (bme.performReading()) {
      //   data = SensorData{millis(), "BME680", bme.temperature, bme.humidity, bme.pressure / 100.0F, (float)bme.gas_resistance};
      //   xQueueSend(dataQueue, &data, portMAX_DELAY);
      // }

      xSemaphoreGive(i2cMutex);
    }
    // UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    // Serial.print("Spazio Stack rimanente nel TaskReadI2CSensors: ");
    // Serial.println(uxHighWaterMark);
  }
}

// ------------------------------------------------------------------
// TASK 2: Lettura DHT22
// ------------------------------------------------------------------
void vTaskReadDHT22(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(SAMPLING_FREQUENCY_MS); 

  SensorData data;

  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      data = SensorData{millis(), "DHT22", t, h, 0, 0};
      xQueueSend(dataQueue, &data, portMAX_DELAY);
    }
    UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    Serial.print("Spazio Stack rimanente nel TaskReadDHT22: ");
    Serial.println(uxHighWaterMark);
  }
}

// ------------------------------------------------------------------
// TASK 3: Lettura MQ2 (Analogico)
// ------------------------------------------------------------------
void vTaskReadMQ2(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(SAMPLING_FREQUENCY_MS); // Più veloce, ogni secondo

  SensorData data;

  for (;;) {
    //vTaskDelayUntil(&xLastWakeTime, xFrequency);

    int analogValue = analogRead(MQ2_PIN);
    data = SensorData{millis(), "MQ2", 0, 0, 0, (float)analogValue};
    // xQueueOverwrite(dataQueue, &data, portMAX_DELAY);

    File file = LittleFS.open("/dati_mq2.csv", FILE_APPEND);
      if (file) {
        file.println(csvBuffer);
        file.close();
      } else {
        Serial.println("Errore nell'apertura del file CSV per l'aggiunta!");
      }
    
    // UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    // Serial.print("Spazio Stack rimanente nel TaskReadMQ2: ");
    // Serial.println(uxHighWaterMark);
  }
}

// ------------------------------------------------------------------
// TASK 4: Data Logger (Scrive su memoria Flash LittleFS)
// ------------------------------------------------------------------
void vTaskDataLogger(void *pvParameters) {
  SensorData receivedData;
  char csvBuffer[256];

  for (;;) {
    // Il task rimane in attesa (bloccato) finché non arriva un dato nella Queue
    if (xQueueReceive(dataQueue, &receivedData, portMAX_DELAY) == pdPASS) {
      
      // Formatta la stringa CSV
      snprintf(csvBuffer, sizeof(csvBuffer), "%lu,%s,%.2f,%.2f,%.2f,%.2f",
               receivedData.timestamp,
               receivedData.sensorName,
               receivedData.temperature,
               receivedData.humidity,
               receivedData.pressure,
               receivedData.gas);

      // Stampa a monitor seriale per debug
      Serial.println(csvBuffer);

      // Scrivi su file in modalità APPEND
      File file = LittleFS.open("/dati_sensori.csv", FILE_APPEND);
      if (file) {
        file.println(csvBuffer);
        file.close();
      } else {
        Serial.println("Errore nell'apertura del file CSV per l'aggiunta!");
      }
    }
    // UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    // Serial.print("Spazio Stack rimanente nel Logger: ");
    // Serial.println(uxHighWaterMark);
  }
}