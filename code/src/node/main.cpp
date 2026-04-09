#include <Arduino.h>
#include <anomalies.hpp>
#include "lora_node.hpp"
#include "states.hpp"
#include "utils.hpp"
#include "simulation.hpp"
#include "file_simulation.hpp"
#include "sensors.hpp"



// Define a bit to represent the sensor event (Bit 0)
#define SENSOR_SAMPLED_BIT ( 1UL << 0 ) 

// Define sensing mode between simulation, simulation from files and real enviroment sensing
#define ENVIROMENT_SENSING_MODE 0
#define FILE_SIMULATION_MODE 1
#define SIMULATION_MODE 2
#define SENSING_MODE ENVIROMENT_SENSING_MODE 

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




void TaskSampleSensors(void* pvParameters){
    sensors_sample_t results;

    for(;;){
        if(SENSING_MODE == ENVIROMENT_SENSING_MODE)
            results = sens_enviroment();
        else if (SENSING_MODE == FILE_SIMULATION_MODE)
            results = sens_file();
        else if (SENSING_MODE == SIMULATION_MODE)
            results = sens_simulation();


        xQueueOverwrite(sens_result, &results);
        xEventGroupSetBits(sensing_event, SENSOR_SAMPLED_BIT);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
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
    if (SENSING_MODE == ENVIROMENT_SENSING_MODE){
        sensors_begin();
    }
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
