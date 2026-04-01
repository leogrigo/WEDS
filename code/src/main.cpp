#include <Arduino.h>

constexpr float PI_F = 3.14159265f;

typedef struct sensors_sample_t{
    float temp;
    float hum;
    float press;
    float pm25;
} sensors_sample_t;

typedef struct anomaly_result_t {
    float temp_baseline;
    float hum_baseline;
    float smoke_baseline;

    float temp_score;
    float hum_score;
    float smoke_score;

    float global_score;
} anomaly_result_t;

typedef struct risk_result_t {
    float global_score;
} risk_result_t;

// Define a bit to represent the sensor event (Bit 0)
#define SENSOR_SAMPLED_BIT ( 1UL << 0 ) 

// Queue and Event Group handles
QueueHandle_t sens_result = xQueueCreate(1, sizeof(sensors_sample_t)); // Queue to hold the latest sensor sample
EventGroupHandle_t sensing_event = xEventGroupCreate(); // Event group to signal sensor sampling
QueueHandle_t anomaly_queue = xQueueCreate(1, sizeof(anomaly_result_t)); // Queue to hold the latest anomaly score calculated
QueueHandle_t risk_queue = xQueueCreate(1, sizeof(risk_result_t)); // Queue to hold the latest risk score calculated

// put function declarations here:
void TaskSampleSensors(void* pvParameters);
float sampleNormal(float mean, float stddev);


void printSample(sensors_sample_t sample){
  Serial.print("Temp: ");
  Serial.print(sample.temp);
  Serial.print(" C, Hum: ");
  Serial.print(sample.hum);
  Serial.print(" %, Press: ");
  Serial.print(sample.press);
  Serial.print(" kPa, PM2.5: ");
  Serial.print(sample.pm25);
  Serial.println(" ug/m3");
}

// put function definitions here:
float sampleNormal(float mean, float stddev){
  float u1 = random(1, 10001) / 10001.0f;
  float u2 = random(0, 10000) / 10000.0f;
  
  float radius = sqrtf(-2.0f * logf(u1));
  float angle = 2.0f * PI_F * u2;
  float z0 = radius * cosf(angle);
  
  return mean + z0 * stddev;
}

void TaskSampleSensors(void* pvParameters){
  sensors_sample_t results;
  for(;;){
    results.temp = sampleNormal(30.0f, 1.5f);
    results.hum = sampleNormal(76.4f, 4.0f);
    results.press = sampleNormal(0.6f, 0.05f);
    results.pm25 = sampleNormal(16.7f, 2.5f);
    
    printSample(results);
    xQueueOverwrite(sens_result, &results);
    xEventGroupSetBits(sensing_event, SENSOR_SAMPLED_BIT);
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// Exponential Moving Average Filter (EMA)
template <uint8_t K, class uint_t = uint16_t>
class EMA {
  public:
    /// Update the filter with the given input and return the filtered output.
    uint_t operator()(uint_t input) {
        state += input;
        uint_t output = (state + half) >> K;
        state -= output;
        return output;
      }
      
      static_assert(
        uint_t(0) < uint_t(-1),
        "The uint_t type should be an unsigned integer, otherwise, "
        "the division using bit shifts is invalid.");
        
    /// Fixed point representation of one half, used for rounding.
    constexpr static uint_t half = 1 << (K - 1);
    
    private:
    uint_t state = 0;
};
  
  
  
void TaskAnomalyDetection(void* pvParameters){
    sensors_sample_t sample;
    anomaly_result_t result;
    EMA<4> temp_filter; // Example: K=4 for a smoother response
    EMA<4> hum_filter;
    EMA<4> smoke_filter;
    
    for(;;){
      // Wait for the sensor bit to be set
      EventBits_t uxBits = xEventGroupWaitBits(
        sensing_event,   // The event group handle
        SENSOR_SAMPLED_BIT,  // The bit(s) to wait for
        pdTRUE,              // xClearOnExit: Clear the bit when returning
        pdFALSE,             // xWaitForAllBits: Doesn't matter for a single bit
        portMAX_DELAY        // Wait indefinitely
      );
      
      // Check if our specific bit was set (useful if waiting on multiple bits)
      if( (uxBits & SENSOR_SAMPLED_BIT) != 0 ) {
        // The sensor was sampled! React to the event here
        // Read the latest sensor data from the queue
        xQueuePeek(sens_result, &sample, 0); // Peek to get the latest sample without removing it from the queue
        // Update filters with new sensor data
        result.temp_baseline = temp_filter(sample.temp);
        result.hum_baseline = hum_filter(sample.hum);
        result.smoke_baseline = smoke_filter(sample.pm25);
        
        // Calculate anomaly scores (simple example)
        result.temp_score = abs(sample.temp - result.temp_baseline);
        result.hum_score = abs(sample.hum - result.hum_baseline);
        result.smoke_score = abs(sample.pm25 - result.smoke_baseline);
        
        // Combine scores into a global score (simple average)
        result.global_score = (result.temp_score + result.hum_score + result.smoke_score) / 3.0f;
        
        // Here you can add code to handle the anomaly results, e.g., log them or trigger alerts
        xQueueOverwrite(anomaly_queue, &result);            
      }
    }
}
  
  
void TaskRiskDetection(void* pvParameters){
    risk_result_t risk_result;
    sensors_sample_t sample;
    
    for(;;){
        EventBits_t uxBits = xEventGroupWaitBits(
            sensing_event,   // The event group handle
            SENSOR_SAMPLED_BIT,  // The bit(s) to wait for
            pdTRUE,              // xClearOnExit: Clear the bit when returning
            pdFALSE,             // xWaitForAllBits: Doesn't matter for a single bit
            portMAX_DELAY        // Wait indefinitely
          );
          
          if( (uxBits & SENSOR_SAMPLED_BIT) != 0 ) {
            // Read the latest sensor data from the queue
            xQueuePeek(sens_result, &sample, 0); // Peek to get the latest sample without removing it from the queue
            
            // Simple risk calculation based on sensor data (example logic)
            risk_result.global_score = (sample.temp / 50.0f) + (sample.hum / 100.0f) + (sample.pm25 / 500.0f);
            
            // Here you can add code to handle the risk results, e.g., log them or trigger alerts
            xQueueOverwrite(risk_queue, &risk_result);
          }
        }
}
      
void TaskMonitoring(void* pvParameters){
    anomaly_result_t anomaly_result;
    risk_result_t risk_result;

    for(;;){
        // Wait for new anomaly results
        if(xQueueReceive(anomaly_queue, &anomaly_result, 5000) == pdPASS) {
        // Process the anomaly result (e.g., log it or trigger alerts)
        Serial.print("Anomaly Global Score: ");
        Serial.println(anomaly_result.global_score);
        // Print individual scores for debugging
        Serial.print("Temp Score: ");
        Serial.print(anomaly_result.temp_score);
        Serial.print(" Hum Score: ");
        Serial.print(anomaly_result.hum_score);
        Serial.print(" Smoke Score: ");
        Serial.println(anomaly_result.smoke_score);
        
        
        }
        
        // Wait for new risk results
        if(xQueueReceive(risk_queue, &risk_result, 5000) == pdPASS) {
        // Process the risk result (e.g., log it or trigger alerts)
        Serial.print("Risk Global Score: ");
        Serial.println(risk_result.global_score);
        }
    }
}
      
void setup() {
// put your setup code here, to run once:
    Serial.begin(115200);
    delay(500);
    randomSeed(micros());
    xTaskCreate(TaskSampleSensors, "Sample Sensors Task", 2048, NULL, 1, NULL);
    xTaskCreate(TaskAnomalyDetection, "Anomaly Detection Task", 2048, NULL, 1, NULL);
    xTaskCreate(TaskRiskDetection, "Risk Detection Task", 2048, NULL, 1, NULL);
    xTaskCreate(TaskMonitoring, "Monitoring Task", 2048, NULL, 1, NULL);


}

void loop() {
// put your main code here, to run repeatedly:
}