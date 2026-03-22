
#include "utilities.h"
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb
#define SerialAT Serial1

// See all AT commands, if wanted
#define DUMP_AT_COMMANDS
#define TINY_GSM_MODEM_SIM7080
#include <TinyGsmClient.h>

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
#include "AT_utils.h" // run_AT_cmd and wait_respose_wrap

XPowersPMU  PMU;

bool checkSignalQuality() {
  Serial.println("checkSignalQuality");
  while (true){  
    Serial1.println("AT+CSQ");
    delay(500);
  
    if (Serial1.available()) {
      String response = Serial1.readString();
      Serial.print("Signal Response: ");
      Serial.println(response);
      
      // Check for the "99" (no signal) pattern
      if (response.indexOf("+CSQ: 99") != -1) {
        Serial.println("Warning: No signal (99). Check antenna.");
      } else {
        return true;
        Serial.println("OK -----------\n\n");
      }
    } else {
      Serial.print("Serial1 not available");
    }
  }
  return false;
  Serial.println("FAILED -----------\n\n");
}

bool PMU_setup(){
    Serial.println("PMU_setup");
    /*********************************
     *  step 1 : Initialize power chip,
     *  turn on modem and gps antenna power channel
    ***********************************/
    if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        Serial.println("Failed to initialize power.....");
        assert(0);
    }

    // Set the led light on to indicate that the board has been turned on
    PMU.setChargingLedMode(XPOWERS_CHG_LED_ON);


    // Turn off other unused power domains
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

    // If it is a power cycle, turn off the modem power. Then restart it
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED ) {
        PMU.disableDC3();
        // Wait a minute
        delay(200);
    }

    //! Do not turn off BLDO1, which controls the 3.3V power supply for level conversion.
    //! If it is turned off, it will not be able to communicate with the modem normally
    PMU.setBLDO1Voltage(3300);    // Set the power supply for level conversion to 3300mV
    PMU.enableBLDO1();

    // Set the working voltage of the modem, please do not modify the parameters
    PMU.setDC3Voltage(3000);    // SIM7080 Modem main power channel 2700~ 3400V
    PMU.enableDC3();

    // Modem GPS Power channel
    PMU.setBLDO2Voltage(3300);
    PMU.enableBLDO2();      // The antenna power must be turned on to use the GPS function

    return true;
    Serial.println("OK -----------\n\n");
}

bool basic_modem_setup(TinyGsm& modem){

    Serial1.begin(115200, SERIAL_8N1, BOARD_MODEM_RXD_PIN, BOARD_MODEM_TXD_PIN);

    pinMode(BOARD_MODEM_PWR_PIN, OUTPUT);

    int retry = 100; // start with the toggle
    Serial.print("Check modem");
    while (!modem.testAT(1000)) {
        Serial.print(".");
        if (retry++ > 5) {
            Serial.println("Toggle modem power");
            // Pull down PWRKEY for more than 1 second according to manual requirements
            digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
            delay(100);
            digitalWrite(BOARD_MODEM_PWR_PIN, HIGH);
            delay(1000);
            digitalWrite(BOARD_MODEM_PWR_PIN, LOW);
            retry = 0;
        }
    }

   

  //// 1. Force the modem to use both LTE-M and NB-IoT
  //run_AT_cmd(modem, "+CNMP=38"); 
  //delay(500);
  //
  //// 2. Set the band mask to "all" (or European standard)
  //// Note: This command allows the modem to search all common bands
  //run_AT_cmd(modem,"+CBAND=ALL"); 
  //delay(500);
  //
  //// 3. Reset the modem to apply band changes
  //run_AT_cmd(modem,"+CFUN=0"); // Set to minimum functionality
  //delay(1000);
  //run_AT_cmd(modem,"+CFUN=1"); // Set to full functionality

  //if (checkSignalQuality()){
  //  Serial.println("OK -----------\n\n");
  //  return true;
  //}
  //Serial.println("FAILED -----------\n\n");
  return false;
}

bool connect_GSM_1nce(TinyGsm& modem, int retries=10){
  Serial.println("connect_GSM_1nce");
  if (!modem.restart()) {
    Serial.println("Failed to restart modem!");
    Serial.println("FAILED -----------\n\n");
    return false;
  }
  // 'iot.1nce.net' standard global APN.
  Serial.println("Waiting for network registration...");
  if (!modem.waitForNetwork(60000)) {
    Serial.println("Network registration timed out.");
    Serial.println("FAILED -----------\n\n");
    return false;
  }
  Serial.println("Network registered.");

  for (int i = 0; i < retries; i++) {
    Serial.printf("Connecting to 1NCE (attempt %d/%d)...\n", i + 1, retries);
    if (modem.gprsConnect("iot.1nce.net", "", "")) {
      Serial.println("Connected to 1NCE!");
      Serial.print("IP Address: ");
      Serial.println(modem.localIP());
      Serial.println("OK -----------\n\n");
      return true;
    }
    Serial.println("GPRS Connect failed, retrying...");
    delay(5000);
  }
  Serial.println("GPRS Connect failed after all retries.");
  Serial.println("FAILED -----------\n\n");
  return false;
}


bool configure_endpoint(TinyGsm& modem){
    Serial.println("configure_endpoint");
    run_AT_cmd(modem, "+SMCONF=\"cleanss\",1");
    run_AT_cmd(modem, "+SMCONF=\"CLIENTID\",\"T-SIM7080G_01\"");
    run_AT_cmd(modem, "+SMCONF=\"URL\",\"a3fu7j5avgf87g-ats.iot.eu-west-3.amazonaws.com\",8883");
    Serial.println("OK -----------\n\n");
    return true;
}

