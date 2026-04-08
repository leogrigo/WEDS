#include "anomalies.hpp"

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