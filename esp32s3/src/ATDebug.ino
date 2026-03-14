#include <Arduino.h>
#include "setup.h"

TinyGsm modem(SerialAT);


void setup()
{
    Serial.begin(115200);

    PMU_setup();
    basic_modem_setup(modem);

    connect_GSM_1nce(modem);
}

void loop()
{
    Serial.print("loop ");
    delay(1000);
}


















