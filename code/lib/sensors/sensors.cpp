#include "sensors.hpp"
#include <Arduino.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>


Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;

void sensors_begin(){
    if (!Wire.begin(I2C_SDA, I2C_SCL)) Serial.println("Wire Initializing Error!");
    pinMode(MQ2_PIN, INPUT);
    if (!aht.begin()) Serial.println("Error AHT20 begin!");
    if (!bmp.begin()) Serial.println("Error BMP280 begin!");
}

void printSample(sensors_sample_t sample){
    Serial.print("Temp: ");
    Serial.print(sample.temp);
    Serial.print("°C Hum: ");
    Serial.print(sample.hum);
    Serial.print("% Press: ");
    Serial.print(sample.press);
    Serial.print("bar Gas: ");
    Serial.print(sample.gas_r*100/4095.0f);
    Serial.println("%");
}

sensors_sample_t sens_enviroment(){
    Serial.println("Sensor reached!");
    sensors_event_t humidity, temp;
    float temp2 = bmp.readTemperature();
    float press = bmp.readPressure();
    aht.getEvent(&humidity, &temp);

    sensors_sample_t ris = {temp2, humidity.relative_humidity, press, (float)analogRead(MQ2_PIN)};
    printSample(ris);
    return ris;
}
