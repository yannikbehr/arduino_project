#ifndef __TEMP_SENSOR_H__
#define __TEMP_SENSOR_H__

#include "Arduino.h"

class TempSensor {
 public:
    enum SensorType {BlackGreyPlastic, MeatSensor};

 private:
    // Voltage going in temp sensor
    int m_Vin = 5;
    // known resistor in Ohm
    float m_R1 = 10000.0;
    int m_pin;
    SensorType m_type;
    
 public:
    TempSensor(int Vin=5, float R1=10000.0, SensorType type = BlackGreyPlastic, int pin=A0);

    float measureResistor(bool verb);

    float linInterpol(float x1, float x2, float y1, float y2, float x);

    float resistanceToTemp(float resistance);

    float tmpSensor();
};

#endif
