#pragma once

#include "board.h"

#define TINY_GSM_RX_BUFFER 1024
#define DUMP_AT_COMMANDS
#define TINY_GSM_MODEM_SIM7080
#include <TinyGsmClient.h>
#include <StreamDebugger.h>

#include "at_utils.h"

StreamDebugger debugger(Serial1, Serial);
#define SerialAT debugger

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

XPowersPMU PMU;

bool PMU_setup() {
    Serial.println("PMU_setup");
    if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        Serial.println("Failed to initialize power.....");
        assert(0);
    }

    PMU.setChargingLedMode(XPOWERS_CHG_LED_ON);

    PMU.disableDC2();
    PMU.disableDC4();
    PMU.disableDC5();
    PMU.disableALDO1();
    PMU.disableALDO2();
    PMU.disableALDO3();
    PMU.disableALDO4();
    PMU.disableBLDO2();
    PMU.disableCPUSLDO();
    PMU.disableDLDO1();
    PMU.disableDLDO2();

    // ESP32S3 power supply cannot be turned off
    // PMU.disableDC1();

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
        PMU.disableDC3();
        delay(200);
    }

    // BLDO1 controls 3.3V level conversion — do not disable
    PMU.setBLDO1Voltage(3300);
    PMU.enableBLDO1();

    PMU.setDC3Voltage(3000);  // SIM7080 main power (2700–3400mV)
    PMU.enableDC3();

    PMU.setBLDO2Voltage(3300);
    PMU.enableBLDO2();  // GPS antenna power

    return true;
}

bool basic_modem_setup(TinyGsm& modem) {
    Serial.println("basic_modem_setup");
    Serial1.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);
    pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);

    int retry = 100;
    Serial.print("Check modem");
    while (!modem.testAT(1000)) {
        Serial.print(".");
        if (retry++ > 5) {
            Serial.println("Toggle modem power");
            digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
            delay(100);
            digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
            delay(1000);
            digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
            retry = 0;
        }
    }
    return true;
}

bool connect_GSM_1nce(TinyGsm& modem, int retries = 10) {
    Serial.println("connect_GSM_1nce");
    if (!modem.restart()) {
        Serial.println("Failed to restart modem!");
        return false;
    }

    Serial.println("Waiting for network registration...");
    if (!modem.waitForNetwork(60000)) {
        Serial.println("Network registration timed out.");
        return false;
    }
    Serial.println("Network registered.");

    for (int i = 0; i < retries; i++) {
        Serial.printf("Connecting to 1NCE (attempt %d/%d)...\n", i + 1, retries);
        if (modem.gprsConnect("iot.1nce.net", "", "")) {
            Serial.println("Connected to 1NCE!");
            Serial.print("IP Address: ");
            Serial.println(modem.localIP());
            return true;
        }
        Serial.println("GPRS Connect failed, retrying...");
        delay(5000);
    }

    Serial.println("GPRS Connect failed after all retries.");
    return false;
}
