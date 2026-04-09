#include "file_simulation.hpp"
#include <Arduino.h>

sensors_sample_t sens_file(){
    Serial.println("File_simulation reached!");
    return {0.0f, 0.0f, 0.0f, 0.0f};
}