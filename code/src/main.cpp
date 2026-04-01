#include <Arduino.h>

constexpr float PI_F = 3.14159265f;
constexpr uint8_t ANOMALY_WARMUP_SAMPLES = 20;

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

    float temp_stddev;
    float hum_stddev;
    float smoke_stddev;

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


void printSample(sensors_sample_t sample){
  Serial.println("==========================================");
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

float sampleNormal(float mean, float stddev){
  float u1 = random(1, 10001) / 10001.0f;
  float u2 = random(0, 10000) / 10000.0f;
  
  float radius = sqrtf(-2.0f * logf(u1));
  float angle = 2.0f * PI_F * u2;
  float z0 = radius * cosf(angle);
  
  return mean + z0 * stddev;
}

float sampleUniform(float min_val, float max_val){
    float u = random(0, 10001) / 10000.0f;
    return min_val + (max_val - min_val) * u;
}

float computeZScore(float sample, float baseline, float stddev){
  constexpr float min_stddev = 0.001f;
  float safe_stddev = fmaxf(stddev, min_stddev);
  return (sample - baseline) / safe_stddev;
}

float positivePart(float x){
  return (x > 0.0f) ? x : 0.0f;
}

float directionalTempScore(float z){
  // temperatura alta = rischio
  return positivePart(z);
}

float directionalHumScore(float z){
  // umidità bassa = rischio
  return positivePart(-z);
}

float directionalSmokeScore(float z){
  // PM2.5 alta = rischio
  return positivePart(z);
}


enum sim_mode_t {
    SIM_MODE_NORMAL,
    SIM_MODE_DRY_PERIOD,
    SIM_MODE_SMOKE_SPIKE,
    SIM_MODE_FIRE_EVENT,
    SIM_MODE_SENSOR_FAULT
};
// Choose the scenario you want to test after warmup
constexpr sim_mode_t SELECTED_MODE = SIM_MODE_FIRE_EVENT;

typedef struct sim_state_t {
    uint32_t tick;
    uint32_t mode_tick;          // counts samples after warmup
    bool in_warmup;

    // Sensor fault support
    bool fault_initialized;
    float fault_temp_value;
    float fault_hum_value;
    float fault_press_value;
    float fault_pm25_value;
} sim_state_t;

void updateSimulationState(sim_state_t* s){
    s->tick++;

    if (s->tick <= ANOMALY_WARMUP_SAMPLES) {
        s->in_warmup = true;
        s->mode_tick = 0;
    } else {
        s->in_warmup = false;
        s->mode_tick++;
    }
}

sensors_sample_t generateSimulatedSample(sim_state_t* s){
    sensors_sample_t sample;

    float t = (float)s->tick;
    float mt = (float)s->mode_tick;

    // -----------------------------
    // Base "normal" environment
    // -----------------------------
    float temp_base_cycle = 1.0f * sinf(0.10f * t);
    float hum_base_cycle  = -3.0f * sinf(0.10f * t);
    float pm_base_cycle   = 0.4f * sinf(0.06f * t);

    float temp_noise  = sampleNormal(0.0f, 0.35f);
    float hum_noise   = sampleNormal(0.0f, 1.0f);
    float press_noise = sampleNormal(0.0f, 0.02f);
    float pm_noise    = sampleNormal(0.0f, 0.8f);

    float temp = 30.0f + temp_base_cycle + temp_noise;
    float hum  = 76.0f + hum_base_cycle + hum_noise;
    float press = 0.60f + press_noise;
    float pm25 = 16.0f + pm_base_cycle + pm_noise;

    // During warmup always stay in normal mode
    if (s->in_warmup) {
        sample.temp = temp;
        sample.hum = hum;
        sample.press = press;
        sample.pm25 = pm25;
        return sample;
    }

    // -----------------------------
    // Scenario after warmup
    // -----------------------------
    switch (SELECTED_MODE) {

        case SIM_MODE_NORMAL:
            // keep normal behavior
            break;

        case SIM_MODE_DRY_PERIOD: {
            // slow temperature rise and slow humidity decrease
            float dry_temp_ramp = 0.08f * mt;   // slowly rising
            float dry_hum_drop  = 0.25f * mt;   // slowly decreasing

            // Optional clamp so it does not become absurd
            if (dry_temp_ramp > 6.0f) dry_temp_ramp = 6.0f;
            if (dry_hum_drop > 18.0f) dry_hum_drop = 18.0f;

            temp += dry_temp_ramp;
            hum  -= dry_hum_drop;
            break;
        }

        case SIM_MODE_SMOKE_SPIKE: {
            // one short particulate spike after a few post-warmup samples
            if (s->mode_tick >= 5 && s->mode_tick <= 8) {
                pm25 += 12.0f;
            }
            break;
        }

        case SIM_MODE_FIRE_EVENT: {
            // multi-sensor coherent event, gradual but significant
            // starts a few samples after warmup and lasts ~12 samples
            if (s->mode_tick >= 5 && s->mode_tick <= 16) {
                float phase = (float)(s->mode_tick - 5) / 11.0f; // 0..1
                if (phase < 0.0f) phase = 0.0f;
                if (phase > 1.0f) phase = 1.0f;

                temp += 2.0f + 4.0f * phase;
                hum  -= 4.0f + 8.0f * phase;
                pm25 += 4.0f + 10.0f * phase;
            }
            break;
        }

        case SIM_MODE_SENSOR_FAULT: {
            // Example: PM2.5 sensor gets stuck after warmup
            if (!s->fault_initialized) {
                s->fault_temp_value = temp;
                s->fault_hum_value = hum;
                s->fault_press_value = press;
                s->fault_pm25_value = pm25;
                s->fault_initialized = true;
            }

            // freeze PM2.5 only; others continue normally
            pm25 = s->fault_pm25_value;

            // You can alternatively freeze temp/hum instead if you want:
            // temp = s->fault_temp_value;
            // hum = s->fault_hum_value;
            break;
        }

        default:
            break;
    }

    sample.temp = temp;
    sample.hum = hum;
    sample.press = press;
    sample.pm25 = pm25;

    return sample;
}


sim_state_t sim_state = {
    0,      // tick
    0,      // mode_tick
    true,   // in_warmup
    false,  // fault_initialized
    0.0f,   // fault_temp_value
    0.0f,   // fault_hum_value
    0.0f,   // fault_press_value
    0.0f    // fault_pm25_value
};

void TaskSampleSensors(void* pvParameters){
    sensors_sample_t results;

    for(;;){
        updateSimulationState(&sim_state);
        results = generateSimulatedSample(&sim_state);

        printSample(results);

        Serial.print("Simulation phase: ");
        if (sim_state.in_warmup) {
            Serial.println("WARMUP_NORMAL");
        } else {
            switch (SELECTED_MODE) {
                case SIM_MODE_NORMAL:
                    Serial.println("NORMAL");
                    break;
                case SIM_MODE_DRY_PERIOD:
                    Serial.println("DRY_PERIOD");
                    break;
                case SIM_MODE_SMOKE_SPIKE:
                    Serial.println("SMOKE_SPIKE");
                    break;
                case SIM_MODE_FIRE_EVENT:
                    Serial.println("FIRE_EVENT");
                    break;
                case SIM_MODE_SENSOR_FAULT:
                    Serial.println("SENSOR_FAULT");
                    break;
                default:
                    Serial.println("UNKNOWN");
                    break;
            }
        }

        xQueueOverwrite(sens_result, &results);
        xEventGroupSetBits(sensing_event, SENSOR_SAMPLED_BIT);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// Exponential Moving Average Filter (EMA)
template <uint8_t K>
class EMA {
  public:
    float operator()(float input) {
        if (!initialized) {
            state = input;
            initialized = true;
            return input;
        }

        constexpr float alpha = 1.0f / (1 << K);
        state += alpha * (input - state);
        return state;
    }

  private:
    bool initialized = false;
    float state = 0.0f;
};
  
  
void printAnomalyResults(anomaly_result_t result){
    Serial.println("Anomaly results:");
    Serial.println("\tBaselines:");
    Serial.println("\t\tTemp_baseline: " + String(result.temp_baseline));
    Serial.println("\t\tHum_baseline: " + String(result.hum_baseline));
    Serial.println("\t\tSmoke_baseline: " + String(result.smoke_baseline));
    Serial.println("\tStandard deviations:");
    Serial.println("\t\tTemp_stddev: " + String(result.temp_stddev));
    Serial.println("\t\tHum_stddev: " + String(result.hum_stddev));
    Serial.println("\t\tSmoke_stddev: " + String(result.smoke_stddev));
    Serial.println("\tScores:");
    Serial.println("\t\tTemp_score: " + String(result.temp_score));
    Serial.println("\t\tHum_score: " + String(result.hum_score));
    Serial.println("\t\tSmoke_score: " + String(result.smoke_score));
    Serial.println("\t\tGlobal_score: " + String(result.global_score));

    
}

// TO DO: Fix number of samples for initial baseline, fix weights for global score, adaptive aplha (state based + isteresi) bisogna evitare che l'evento venga assorbito troppo velocemente

void TaskAnomalyDetection(void* pvParameters){
    sensors_sample_t sample = {0};
    anomaly_result_t result = {0};
    EMA<4> temp_filter;
    EMA<4> hum_filter;
    EMA<4> smoke_filter;
    EMA<4> temp_variance_filter;
    EMA<4> hum_variance_filter;
    EMA<4> smoke_variance_filter;
    uint8_t samples_seen = 0;
    
    for(;;){
      EventBits_t uxBits = xEventGroupWaitBits(
        sensing_event,
        SENSOR_SAMPLED_BIT,
        pdTRUE,
        pdFALSE,
        portMAX_DELAY
      );
      
      if( (uxBits & SENSOR_SAMPLED_BIT) != 0 ) {
        xQueuePeek(sens_result, &sample, 0);

        samples_seen++;

        // First sample: initialize filters, no anomaly score yet
        if (samples_seen == 1) {
          result.temp_baseline = temp_filter(sample.temp);
          result.hum_baseline = hum_filter(sample.hum);
          result.smoke_baseline = smoke_filter(sample.pm25);

          result.temp_stddev = 0.0f;
          result.hum_stddev = 0.0f;
          result.smoke_stddev = 0.0f;

          result.temp_score = 0.0f;
          result.hum_score = 0.0f;
          result.smoke_score = 0.0f;
          result.global_score = 0.0f;

          Serial.println("Warming up anomaly detection... ");
          printAnomalyResults(result);
          xQueueOverwrite(anomaly_queue, &result);
          continue;
        }

        // Use PREVIOUS baseline/stddev for scoring
        float prev_temp_baseline = result.temp_baseline;
        float prev_hum_baseline = result.hum_baseline;
        float prev_smoke_baseline = result.smoke_baseline;

        float prev_temp_stddev = result.temp_stddev;
        float prev_hum_stddev = result.hum_stddev;
        float prev_smoke_stddev = result.smoke_stddev;

        // Residuals against PREVIOUS baseline
        float temp_residual = sample.temp - prev_temp_baseline;
        float hum_residual = sample.hum - prev_hum_baseline;
        float smoke_residual = sample.pm25 - prev_smoke_baseline;

        if (samples_seen <= ANOMALY_WARMUP_SAMPLES) {
          result.temp_score = 0.0f;
          result.hum_score = 0.0f;
          result.smoke_score = 0.0f;
          result.global_score = 0.0f;
          Serial.println("Warming up anomaly detection... ");
        } else {
          // Raw z-scores computed with PREVIOUS baseline/stddev
          float temp_z = computeZScore(sample.temp, prev_temp_baseline, prev_temp_stddev);
          float hum_z = computeZScore(sample.hum, prev_hum_baseline, prev_hum_stddev);
          float smoke_z = computeZScore(sample.pm25, prev_smoke_baseline, prev_smoke_stddev);

          // Directional scores for wildfire detection:
          // temp high -> risk
          // hum low -> risk
          // pm25 high -> risk
          result.temp_score = directionalTempScore(temp_z);
          result.hum_score = directionalHumScore(hum_z);
          result.smoke_score = directionalSmokeScore(smoke_z);

          // Weighted global score
          constexpr float TEMP_WEIGHT = 0.35f;
          constexpr float HUM_WEIGHT = 0.25f;
          constexpr float SMOKE_WEIGHT = 0.40f;

          result.global_score =
              TEMP_WEIGHT * result.temp_score +
              HUM_WEIGHT * result.hum_score +
              SMOKE_WEIGHT * result.smoke_score;
        }

        // Update stddev using residuals against PREVIOUS baseline
        result.temp_stddev = sqrtf(temp_variance_filter(temp_residual * temp_residual));
        result.hum_stddev = sqrtf(hum_variance_filter(hum_residual * hum_residual));
        result.smoke_stddev = sqrtf(smoke_variance_filter(smoke_residual * smoke_residual));

        // Update baseline AFTER scoring
        result.temp_baseline = temp_filter(sample.temp);
        result.hum_baseline = hum_filter(sample.hum);
        result.smoke_baseline = smoke_filter(sample.pm25);
        
        printAnomalyResults(result);
        xQueueOverwrite(anomaly_queue, &result);            
      }
    }
}



// void TaskAnomalyDetection(void* pvParameters){
//     sensors_sample_t sample = {0};
//     anomaly_result_t result = {0};
//     EMA<4> temp_filter; // Example: K=4 for a smoother response
//     EMA<4> hum_filter;
//     EMA<4> smoke_filter;
//     EMA<4> temp_variance_filter;
//     EMA<4> hum_variance_filter;
//     EMA<4> smoke_variance_filter;
//     uint8_t samples_seen = 0;
    
//     for(;;){
//       // Wait for the sensor bit to be set
//       EventBits_t uxBits = xEventGroupWaitBits(
//         sensing_event,   // The event group handle
//         SENSOR_SAMPLED_BIT,  // The bit(s) to wait for
//         pdTRUE,              // xClearOnExit: Clear the bit when returning
//         pdFALSE,             // xWaitForAllBits: Doesn't matter for a single bit
//         portMAX_DELAY        // Wait indefinitely
//       );
      
//       // Check if our specific bit was set (useful if waiting on multiple bits)
//       if( (uxBits & SENSOR_SAMPLED_BIT) != 0 ) {
//         // The sensor was sampled! React to the event here
//         // Read the latest sensor data from the queue
//         xQueuePeek(sens_result, &sample, 0); // Peek to get the latest sample without removing it from the queue
//         // Update baselines using the EMA filters
//         result.temp_baseline = temp_filter(sample.temp);
//         result.hum_baseline = hum_filter(sample.hum);
//         result.smoke_baseline = smoke_filter(sample.pm25);
//         // TO DO: Save baseline also in a part of memory that resists deep sleep
        
//         float temp_residual = sample.temp - result.temp_baseline;
//         float hum_residual = sample.hum - result.hum_baseline;
//         float smoke_residual = sample.pm25 - result.smoke_baseline;

//         result.temp_stddev = sqrtf(temp_variance_filter(temp_residual * temp_residual));
//         result.hum_stddev = sqrtf(hum_variance_filter(hum_residual * hum_residual));
//         result.smoke_stddev = sqrtf(smoke_variance_filter(smoke_residual * smoke_residual));

//         samples_seen++;
//         if (samples_seen <= ANOMALY_WARMUP_SAMPLES) {
//           result.temp_score = 0.0f;
//           result.hum_score = 0.0f;
//           result.smoke_score = 0.0f;
//           result.global_score = 0.0f;
//           Serial.println("Warming up anomaly detection... ");
//         } else {
//           // Calculate anomaly scores as directional z-scores.
//           result.temp_score = computeZScore(sample.temp, result.temp_baseline, result.temp_stddev);
//           result.hum_score = computeZScore(sample.hum, result.hum_baseline, result.hum_stddev);
//           result.smoke_score = computeZScore(sample.pm25, result.smoke_baseline, result.smoke_stddev);
        
//           // Combine scores into a global score (weighted average)
//           result.global_score = (fabsf(result.temp_score) + fabsf(result.hum_score) + fabsf(result.smoke_score)) / 3.0f;
//         }
        
//         printAnomalyResults(result);
//         // Here you can add code to handle the anomaly results, e.g., log them or trigger alerts
//         xQueueOverwrite(anomaly_queue, &result);            
//       }
//     }
// }
  
  
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
      
// void TaskMonitoring(void* pvParameters){
//     anomaly_result_t anomaly_result;
//     risk_result_t risk_result;

//     for(;;){
//         // Wait for new anomaly results
//         if(xQueueReceive(anomaly_queue, &anomaly_result, 5000) == pdPASS) {
//         // Process the anomaly result (e.g., log it or trigger alerts)
//         // Serial.print("Anomaly Global Score: ");
//         // Serial.println(anomaly_result.global_score);
//         // // Print individual scores for debugging
//         // Serial.print("Temp Score: ");
//         // Serial.print(anomaly_result.temp_score);
//         // Serial.print(" Hum Score: ");
//         // Serial.print(anomaly_result.hum_score);
//         // Serial.print(" Smoke Score: ");
//         // Serial.println(anomaly_result.smoke_score);
        
        
//         }
        
//         // Wait for new risk results
//         if(xQueueReceive(risk_queue, &risk_result, 5000) == pdPASS) {
//         // Process the risk result (e.g., log it or trigger alerts)
//         Serial.print("Risk Global Score: ");
//         Serial.println(risk_result.global_score);
//         }
//     }
// }
      
void setup() {
// put your setup code here, to run once:
    Serial.begin(115200);
    delay(500);
    randomSeed(micros());
    xTaskCreate(TaskSampleSensors, "Sample Sensors Task", 2048, NULL, 1, NULL);
    xTaskCreate(TaskAnomalyDetection, "Anomaly Detection Task", 2048, NULL, 1, NULL);
    xTaskCreate(TaskRiskDetection, "Risk Detection Task", 2048, NULL, 1, NULL);
    // xTaskCreate(TaskMonitoring, "Monitoring Task", 2048, NULL, 1, NULL);


}

void loop() {
// put your main code here, to run repeatedly:
}
