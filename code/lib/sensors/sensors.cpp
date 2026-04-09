#include "sensors.hpp"

sensors_sample_t sens_enviroment(){
    Serial.println("Sensor reached!");
    return {0.0f, 0.0f, 0.0f, 0.0f};
}
