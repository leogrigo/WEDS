#include <Arduino.h>
#include "lora_node.h"


constexpr float PI_F = 3.14159265f;
constexpr uint8_t ANOMALY_WARMUP_SAMPLES = 20;

typedef struct sensors_sample_t{
    float temp;
    float hum;
    float press;
    float gas_r;
} sensors_sample_t;

typedef struct anomaly_result_t {
    float temp_baseline;
    float hum_baseline;
    float gas_baseline;

    float temp_stddev;
    float hum_stddev;
    float gas_stddev;

    float temp_score;
    float hum_score;
    float gas_score;

    float global_score;
} anomaly_result_t;

typedef struct risk_result_t {
    float global_score;
} risk_result_t;


enum anomaly_state {
    ANOMALY_STARTUP,
    ANOMALY_WARMUP,
    NO_ANOMALY,
    ANOMALY_WARNING,
    ANOMALY_ALERT
};
typedef struct node_state_t
{
    float sample_freq; // Samples per minute
    int samples_seen;
    int anomaly_warning_streak;
    anomaly_state an_state;
    sensors_sample_t anomaly_last_proc_sample;
    anomaly_result_t anomaly_last_result;

    /* data */
} node_state_t;


// Define a bit to represent the sensor event (Bit 0)
#define SENSOR_SAMPLED_BIT ( 1UL << 0 ) 

// Queue and Event Group handles
QueueHandle_t sens_result = xQueueCreate(1, sizeof(sensors_sample_t)); // Queue to hold the latest sensor sample
EventGroupHandle_t sensing_event = xEventGroupCreate(); // Event group to signal sensor sampling
QueueHandle_t anomaly_queue = xQueueCreate(1, sizeof(anomaly_result_t)); // Queue to hold the latest anomaly score calculated
QueueHandle_t risk_queue = xQueueCreate(1, sizeof(risk_result_t)); // Queue to hold the latest risk score calculated

node_state_t state = {
    10, // Sample freq (ideally 0.2 per minute but set to 10 for simulation purposes)
    0,
    0,
    ANOMALY_STARTUP,
    {0},
    {0},


    };
// put function declarations here:


void printSample(sensors_sample_t sample){
  Serial.println("==========================================");
  Serial.print("Temp: ");
  Serial.print(sample.temp);
  Serial.print(" C, Hum: ");
  Serial.print(sample.hum);
  Serial.print(" %, Press: ");
  Serial.print(sample.press);
  Serial.print(" kPa, Gas resistance: ");
  Serial.print(sample.gas_r);
  Serial.println(" ohm");
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

float directionalGasScore(float z){
  // resistenza ai gas bassa = rischio
  return positivePart(-z);
}

// ================== SIMULATION CODE ==================

enum sim_mode_t {
    SIM_MODE_NORMAL,
    SIM_MODE_DRY_PERIOD,
    SIM_MODE_GAS_DROP,
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
    float fault_gas_r_value;
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
    float gas_base_cycle  = -80.0f * sinf(0.06f * t);

    float temp_noise  = sampleNormal(0.0f, 0.35f);
    float hum_noise   = sampleNormal(0.0f, 1.0f);
    float press_noise = sampleNormal(0.0f, 0.02f);
    float gas_noise   = sampleNormal(0.0f, 120.0f);

    float temp = 30.0f + temp_base_cycle + temp_noise;
    float hum  = 76.0f + hum_base_cycle + hum_noise;
    float press = 0.60f + press_noise;
    float gas_r = 18000.0f + gas_base_cycle + gas_noise;

    // During warmup always stay in normal mode
    if (s->in_warmup) {
        sample.temp = temp;
        sample.hum = hum;
        sample.press = press;
        sample.gas_r = gas_r;
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

        case SIM_MODE_GAS_DROP: {
            // short gas resistance drop after a few post-warmup samples
            if (s->mode_tick >= 5 && s->mode_tick <= 8) {
                gas_r -= 2500.0f;
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
                gas_r -= 1500.0f + 3500.0f * phase;
            }
            break;
        }

        case SIM_MODE_SENSOR_FAULT: {
            // Example: gas resistance sensor gets stuck after warmup
            if (!s->fault_initialized) {
                s->fault_temp_value = temp;
                s->fault_hum_value = hum;
                s->fault_press_value = press;
                s->fault_gas_r_value = gas_r;
                s->fault_initialized = true;
            }

            // freeze gas resistance only; others continue normally
            gas_r = s->fault_gas_r_value;

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
    sample.gas_r = gas_r;

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
    0.0f    // fault_gas_r_value
};

// =====================================================

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
                case SIM_MODE_GAS_DROP:
                    Serial.println("GAS_DROP");
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
    Serial.println("\t\tGas_baseline: " + String(result.gas_baseline));
    Serial.println("\tStandard deviations:");
    Serial.println("\t\tTemp_stddev: " + String(result.temp_stddev));
    Serial.println("\t\tHum_stddev: " + String(result.hum_stddev));
    Serial.println("\t\tGas_stddev: " + String(result.gas_stddev));
    Serial.println("\tScores:");
    Serial.println("\t\tTemp_score: " + String(result.temp_score));
    Serial.println("\t\tHum_score: " + String(result.hum_score));
    Serial.println("\t\tGas_score: " + String(result.gas_score));
    Serial.println("\t\tGlobal_score: " + String(result.global_score));

    
}

// TO DO: Fix number of samples for initial baseline, fix weights for global score, adaptive aplha (state based + isteresi) bisogna evitare che l'evento venga assorbito troppo velocemente
void TaskAnomalyDetection(void* pvParameters){
    sensors_sample_t sample = {0};
    anomaly_result_t result = {0};
    EMA<4> temp_filter;
    EMA<4> hum_filter;
    EMA<4> gas_filter;

    EMA<4> temp_variance_filter;
    EMA<4> hum_variance_filter;
    EMA<4> gas_variance_filter;


    // Persistence counters
    uint8_t warning_counter = 0;
    uint8_t alert_counter = 0;

    // Thresholds
    constexpr float WARNING_THRESHOLD = 1.5f;
    constexpr float ALERT_THRESHOLD   = 2.5f;

    // Minimum stddev per feature
    constexpr float MIN_TEMP_STDDEV = 0.2f;
    constexpr float MIN_HUM_STDDEV  = 1.0f;
    constexpr float MIN_GAS_STDDEV  = 300.0f;

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

        state.samples_seen++;

        // First sample: initialize filters, no anomaly score yet
        if (state.an_state == ANOMALY_STARTUP) {
          result.temp_baseline = temp_filter(sample.temp);
          result.hum_baseline  = hum_filter(sample.hum);
          result.gas_baseline  = gas_filter(sample.gas_r);

          result.temp_stddev = 0.0f;
          result.hum_stddev  = 0.0f;
          result.gas_stddev  = 0.0f;

          result.temp_score = 0.0f;
          result.hum_score  = 0.0f;
          result.gas_score  = 0.0f;
          result.global_score = 0.0f;

          state.anomaly_last_proc_sample = sample;
          state.anomaly_last_result = result;
          state.an_state = ANOMALY_WARMUP;

          Serial.println("Warming up anomaly detection...");
          printAnomalyResults(result);
          xQueueOverwrite(anomaly_queue, &result);
          continue;
        }

        result = state.anomaly_last_result;

        // Use PREVIOUS baseline/stddev for scoring
        float prev_temp_baseline = result.temp_baseline;
        float prev_hum_baseline  = result.hum_baseline;
        float prev_gas_baseline  = result.gas_baseline;

        float prev_temp_stddev = result.temp_stddev;
        float prev_hum_stddev  = result.hum_stddev;
        float prev_gas_stddev  = result.gas_stddev;

        // Residuals against PREVIOUS baseline
        float temp_residual = sample.temp - prev_temp_baseline;
        float hum_residual  = sample.hum  - prev_hum_baseline;
        float gas_residual  = sample.gas_r - prev_gas_baseline;

        // Delta features
        float temp_delta = 0.0f;
        float hum_delta  = 0.0f;
        float gas_delta  = 0.0f;
        float delta_time = 1/state.sample_freq; // in minutes

        temp_delta = (sample.temp - state.anomaly_last_proc_sample.temp) / delta_time;
        hum_delta  = (sample.hum  - state.anomaly_last_proc_sample.hum) / delta_time;
        gas_delta  = (sample.gas_r - state.anomaly_last_proc_sample.gas_r) / delta_time;
        
        
        // Use feature-specific minimum stddev
        float safe_temp_stddev = fmaxf(prev_temp_stddev, MIN_TEMP_STDDEV);
        float safe_hum_stddev  = fmaxf(prev_hum_stddev,  MIN_HUM_STDDEV);
        float safe_gas_stddev  = fmaxf(prev_gas_stddev,  MIN_GAS_STDDEV);

        // Raw z-scores computed with PREVIOUS baseline/stddev
        float temp_z = (sample.temp - prev_temp_baseline) / safe_temp_stddev;
        float hum_z  = (sample.hum  - prev_hum_baseline)  / safe_hum_stddev;
        float gas_z  = (sample.gas_r - prev_gas_baseline) / safe_gas_stddev;

        // Directional scores for wildfire detection
        result.temp_score = directionalTempScore(temp_z);
        result.hum_score  = directionalHumScore(hum_z);
        result.gas_score  = directionalGasScore(gas_z);

        // Weighted global score
        constexpr float TEMP_WEIGHT = 0.35f;
        constexpr float HUM_WEIGHT  = 0.25f;
        constexpr float GAS_WEIGHT  = 0.40f;

        result.global_score =
            TEMP_WEIGHT * result.temp_score +
            HUM_WEIGHT  * result.hum_score +
            GAS_WEIGHT  * result.gas_score;

        // Delta bonus: reward coherent fast changes
        float delta_bonus = 0.0f;

        if (temp_delta > 0.3f)   delta_bonus += 0.3f;   // temp rising
        if (hum_delta < -1.0f)   delta_bonus += 0.3f;   // hum falling
        if (gas_delta < -500.0f) delta_bonus += 0.4f;   // gas resistance falling

        result.global_score += delta_bonus;


        // Update stddev using residuals against PREVIOUS baseline
        result.temp_stddev = sqrtf(temp_variance_filter(temp_residual * temp_residual));
        result.hum_stddev  = sqrtf(hum_variance_filter(hum_residual * hum_residual));
        result.gas_stddev  = sqrtf(gas_variance_filter(gas_residual * gas_residual));

        // Update baseline: freeze baseline if anomaly is detected;
        if (state.an_state < ANOMALY_WARNING) {
                result.temp_baseline = temp_filter(sample.temp);
                result.hum_baseline  = hum_filter(sample.hum);
                result.gas_baseline  = gas_filter(sample.gas_r);
        } else {
                Serial.println("Baseline frozen due to anomaly suspicioun.");
        }

        state.anomaly_last_proc_sample = sample;
        state.anomaly_last_result = result;

        xQueueOverwrite(anomaly_queue, &result);
        printAnomalyResults(result);
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
            constexpr float GAS_REFERENCE = 20000.0f;
            float gas_risk = positivePart((GAS_REFERENCE - sample.gas_r) / GAS_REFERENCE);
            risk_result.global_score = (sample.temp / 50.0f) + (sample.hum / 100.0f) + gas_risk;
            
            // Here you can add code to handle the risk results, e.g., log them or trigger alerts
            xQueueOverwrite(risk_queue, &risk_result);
          }
        }
}
      

float GAS_WARNING_THRESHOLD = 1.5f;
float GLOBAL_WARNING_THRESHOLD = 1.5f;

void SendNodeStateToGateway(){
    LoraComm::NodeStatus lora_status = {
        1,
        static_cast<uint32_t>(state.samples_seen),
        static_cast<uint8_t>(state.an_state),
        state.anomaly_last_proc_sample.temp,
        state.anomaly_last_proc_sample.hum,
        state.anomaly_last_proc_sample.press,
        state.anomaly_last_proc_sample.gas_r,
        state.anomaly_last_result.global_score,
        0.0f
    };

    if (!LoraComm::sendNodeStatus(lora_status)) {
        Serial.println("Gateway update not sent.");
    }
}

// Logic of the anomaly state machine
void ProcessAnomalyResult(anomaly_result_t result){
    if (state.an_state == NO_ANOMALY){
        if (result.gas_score > GAS_WARNING_THRESHOLD || result.global_score > GLOBAL_WARNING_THRESHOLD){
            state.an_state = ANOMALY_WARNING;
            state.anomaly_warning_streak++;
        }
    }
    else if (state.an_state == ANOMALY_WARNING){
        if (result.gas_score < GAS_WARNING_THRESHOLD){
            state.anomaly_warning_streak = max(0, state.anomaly_warning_streak - 2);
        }
        else {
            state.anomaly_warning_streak++;
        }

        if (state.anomaly_warning_streak <= 0) {
            state.an_state = NO_ANOMALY;
            state.anomaly_warning_streak = 0;
        } else if (state.anomaly_warning_streak >= 3) {
            state.an_state = ANOMALY_ALERT;
        }
    }

}

void TaskStateMachine(void* pvParameters){
    anomaly_result_t anomaly_result;
    risk_result_t risk_result;

    for(;;){
        // Wait for new anomaly results
        if(xQueueReceive(anomaly_queue, &anomaly_result, 5000) == pdPASS) {
            if (state.an_state == ANOMALY_WARMUP){
                if (state.samples_seen > ANOMALY_WARMUP_SAMPLES) {
                    state.an_state = NO_ANOMALY;
                    ProcessAnomalyResult(anomaly_result);
                }
                else{
                    Serial.println("Warming up anomaly detection...");
                }
                continue;
            }

            //     // You can print persistence state for debugging
            // Serial.print("Warning counter: ");
            // Serial.println(warning_counter);
            // Serial.print("Alert counter: ");
            // Serial.println(alert_counter);
            

            // Decide if current situation is suspicious
            ProcessAnomalyResult(anomaly_result);
            bool suspicious_event = (state.an_state >= ANOMALY_WARNING);

            if (suspicious_event) {
                SendNodeStateToGateway();
            }

            
        }
            
            // Wait for new risk results
        if(xQueueReceive(risk_queue, &risk_result, 5000) == pdPASS) {
            // TODO: Risk Processing
        }
    }
}
      
void setup() {
// put your setup code here, to run once:
    Serial.begin(115200);
    delay(500);
    randomSeed(micros());
    LoraComm::begin();
    xTaskCreate(TaskSampleSensors, "Sample Sensors Task", 3072, NULL, 1, NULL);
    xTaskCreate(TaskAnomalyDetection, "Anomaly Detection Task", 4096, NULL, 1, NULL);
    xTaskCreate(TaskRiskDetection, "Risk Detection Task", 3072, NULL, 1, NULL);
    xTaskCreate(TaskStateMachine, "State Machine Task", 6144, NULL, 1, NULL);


}

void loop() {
// put your main code here, to run repeatedly:
}
