
#include "TempSensor.h"
// R1: known resistor to compare 
// Vin: voltage 
// pin: inputPin to measure. 
TempSensor::TempSensor(int Vin, float R1, SensorType type, int pin):
  m_Vin(Vin),
  m_R1(R1),
  m_type(type),
  m_pin(pin)
{}

float TempSensor::measureResistor(bool verb) {
  int raw = analogRead(m_pin);
  if (!raw){
    Serial.println("Resistance too high");
    raw = 1;
  }

  /*
   *  + -- TempSensor ---+
   *                     |
   *                     +---- analog in (arduino)
   *                     |  
   *  - -- resistor 1 ---+
   *                     |
   *                     -
   *                    | | - resistor (low pass filter)
   *                     -
   *                     |
   *  - --[capacitor]----+    (low pass filter)
   */
  float Vout = (raw * m_Vin) / 1024.0;
  float R2 = m_R1 * ((m_Vin / Vout) - 1);
  if (verb) {
    Serial.print("Vout: ");
    Serial.println(Vout);
    Serial.print("R2: ");
    Serial.println(R2);
  }
  return R2;
}

float TempSensor::linInterpol(float x1, float x2, float y1, float y2, float x) {
 return (x - x1) / (x2 - x1) * (y2 - y1) + y1;
}

float TempSensor::resistanceToTemp(float resistance) {

  if (m_type == BlackGreyPlastic){
    // Using known resistor 10kOhm
    // Data for temp sensor (gray cable; black head)
    // resistance Ohm 2846 -> 18 ªC
    //                2765 -> 19 ªC
    //                1849 -> 29.5 ªC
    //                1467 -> 37.5 ªC
    //                1403 -> 39 ªC
    //                 824 -> 60.5 ªC
    //                1100 -> 47 ªC
    //                1147 -> 45.5 ªC
    //                1983 -> 27.5 ªC
    /* R plot:
      temp = c(60.5, 47, 45.5, 39, 37.5, 29.5, 27.5, 19, 18)
      res = c(824, 1100, 1147, 1403, 1467, 1849, 1983, 2765, 2846)
      a = matrix(c(log(res), log(res)^2, log(res)^3), ncol=3)
      fit = lm(1/temp ~ a) # linear model

      # plot the fit:
      vals = ((res[1]/10):(res[length(res)]/10))*10
      a2 = matrix(c(log(vals), log(vals)^2, log(vals)^3), ncol=3)
      plot(vals, 1/(a2 %*% fit$coefficients[-1]+fit$coefficients[[1]]), ylim=c(10, 100), type="l")
      lines(res, temp, col="blue")
      fit$coefficients
        (Intercept)          a1          a2          a3 
        -4.35651255  1.89185542 -0.27490372  0.01342537
    */
    float l = log(resistance);
    return 1/(-4.35651255 + 1.89185542*l -0.27490372*l*l + 0.01342537*l*l*l);
  }
  else if (m_type == MeatSensor){
    /*
     287879 Ohm -> 17ºC
     369508 Ohm -> 23ªC
    1133735 Ohm -> 50ºC
    4352174 Ohm -> 95ºC
    Steinhart-Hart equation
    1/Temp = A + B*ln(R) + C*ln(R)^2 + D*ln(R)^3

    // least squares
    res = c(287879, 369508, 1133735, 1551613, 1832076, 1948000, 2338095, 2525641, 2744445, 2825714, 3203226, 3692593, 3996000, 4166667, 4352174, 4352174)
    temp = c(17, 23, 50, 59, 64, 67, 71, 74, 78, 78, 82, 88, 91, 94, 95, 95)
    a = matrix(c(log(res), log(res)^2, log(res)^3), ncol=3)
    fit = lm(1/temp ~ a) # linear model

    # plot the fit:
    vals = ((res[1]/10000):(res[length(res)]/10000))*10000
    a2 = matrix(c(log(vals), log(vals)^2, log(vals)^3), ncol=3)
    plot(vals, 1/(a2 %*% fit$coefficients[-1]+fit$coefficients[[1]]), ylim=c(10, 100), type="l")
    lines(res, temp, col="blue")
    fit$coefficients
     (Intercept)           a1           a2           a3 
    12.213890099 -2.494269404  0.170312289 -0.003883761
    */
    float l = log(resistance);
    return 1/(12.213890099 - 2.494269404*l + 0.170312289*l*l - 0.003883761*l*l*l);
  }
}

float TempSensor::tmpSensor(){
  float sum = 0.0;
  for (int i = 0; i < 10; i++) {
    sum += measureResistor(false) / 10;
    delay(10);
  }
  float temp = resistanceToTemp(sum);
  return temp;
}

