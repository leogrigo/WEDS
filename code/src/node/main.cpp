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

node_state_t state = {
    .node_id = ESP.getEfuseMac(),
    .sample_freq = 30.0f, // 30 samples per minute
    .samples_seen = 0,
    .an_state = ANOMALY_STARTUP,
    .anomaly_last_proc_sample = {0},
    .anomaly_last_result = {0}
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


void TaskAnomalyDetection(void* pvParameters){
    sensors_sample_t sample;
    anomaly_result_t result;

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
        result = calculateAnomalyScore(sample);
        xQueueOverwrite(anomaly_queue, &result);
        printAnomalyResults(result);
      }
    }
}


void SendNodeStateToGateway(){
    if (!LoraComm::sendNodeStatus(state)){
        Serial.println("Failed to send node status to gateway.");
    } 
}

// State machine task: handles state transitions and gateway communication based on anomaly results
void TaskStateMachine(void* pvParameters){
    anomaly_result_t anomaly_result;
    u_int8_t no_anomaly_counter = 0;

    for(;;){
        // Wait for new anomaly results
        if(xQueueReceive(anomaly_queue, &anomaly_result, 5000) == pdPASS) {
            if (state.an_state == ANOMALY_WARMUP){
                // After ANOMALY_WARMUP_SAMPLES samples, exit warmup state (or after stddev < threshold)
                if(state.samples_seen > ANOMALY_WARMUP_SAMPLES){
                    state.an_state = NO_ANOMALY;
                }
            }

            else if (state.an_state == NO_ANOMALY){
                if (anomaly_result.gas_score > GAS_WARNING_THRESHOLD){
                    state.an_state = ANOMALY_WARNING;
                    SendNodeStateToGateway(); // Send update to gateway on anomaly detection
                    no_anomaly_counter = 0;
                }
                else{
                    no_anomaly_counter += 1;
                    if (no_anomaly_counter >= 5){
                        SendNodeStateToGateway(); // Send periodic update to gateway every 5 samples without anomaly
                        no_anomaly_counter = 0;
                    }
                }
            }

            else if (state.an_state == ANOMALY_WARNING){
                if (anomaly_result.gas_score < GAS_WARNING_THRESHOLD){
                    state.an_state = NO_ANOMALY;
                    SendNodeStateToGateway(); // Send update to gateway on anomaly detection
                }
                else{ //TODO: wait for gateway ack
                    SendNodeStateToGateway(); // Send update to gateway on anomaly detection
                }
            }
            
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
    xTaskCreate(TaskSampleSensors, "Sample Sensors Task", 4096, NULL, 1, NULL);
    xTaskCreate(TaskAnomalyDetection, "Anomaly Detection Task", 4096, NULL, 1, NULL);
    xTaskCreate(TaskStateMachine, "State Machine Task", 8192, NULL, 1, NULL);
}

void loop() {
// put your main code here, to run repeatedly:
}
