#include <ESPmDNS.h>
#include "Utils.h"
#include "TempSensor.h"
#include <HX711.h>
#include <Preferences.h> // Using non-volatile storage to store individual values
Preferences preferences;

const int led = 13;
int extLED = 0;


const char server[] = "d2adilqhg7vm5d.cloudfront.net";
const int port = 80 ;

// Modemsetup and pins
#define TINY_GSM_MODEM_SIM800
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26

// Pins heating
#define PIN_HEATING          15

// Temp pins
#define PIN_TEMP1            0

// Constants for temp for tru-components temp sensor with 10 kΩ 3950 K 
const float VCC = 3.3;         // Supply voltage
const float R_FIXED = 100;   // Fixed resistor value (100Ω)

const float TEMP0 = 298.15;       // Reference temperature (25°C in Kelvin)
const float BETA = 3850;       // Beta parameter
const float R0 = 100;        // Resistance at 25°C (100Ω)

//#define SerialMon Serial
#define SerialAT Serial1
#if !defined(TINY_GSM_RX_BUFFER)
#define TINY_GSM_RX_BUFFER 650
#endif

// See all AT commands -> commands send to modem to drive connection / communication, if wanted
// #define DUMP_AT_COMMANDS
#define TINY_GSM_DEBUG Serial
// Add a reception delay - may be needed for a fast processor at a slow baud rate
#define TINY_GSM_YIELD() { delay(2); }

// GSM settings
#define GSM_PIN ""
// 1nce
const char apn[]   = "iot.1nce.net";
const char gprsUser[] = ""; // GPRS User
const char gprsPass[] = ""; // GPRS Password


// SIM card PIN (leave empty, if not defined)
const char simPIN[]   = "";


#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif
//TinyGsmClientSecure client(modem);
TinyGsmClient client(modem);
HttpClient http(client, server, port);

String parse_body(String body){
  if (strstr(body.c_str(), "switch:on")){
    return "on";
  }
  if (strstr(body.c_str(), "switch:off")){
    return "off";
  }
  return "unclear";
}


int counter;
void setup(void) {
  // Non-volatile storage...
  // create reboot counter
  //preferences.begin("my-app", false);
  //counter = preferences.getUInt("counter", 0);
  //preferences.putUInt("counter", ++counter);
  //preferences.end();

  // Modem setup
  // Set your reset, enable, power pins here
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  // pin heating
  pinMode(PIN_HEATING, OUTPUT);
  digitalWrite(PIN_HEATING, LOW);

  // temp pin
  pinMode(PIN_TEMP1, INPUT);

  Serial.begin(115200);
  SerialAT.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX);
  modem.setBaud(9600);
}

float check_temp(int temp_pin){
  int adcValue = analogRead(temp_pin);
  float Vout = adcValue * (VCC / 4095.0);
  float R_therm = R_FIXED * (VCC / Vout - 1);
  // Calculate temperature in Kelvin
  float temperatureK = 1 / (1 / TEMP0 + (1 / BETA) * log(R_therm / R0));

  // Convert Kelvin to Celsius
  float temperatureC = temperatureK - 273.15;

  Serial.println("");
  Serial.print("ADC Value: ");
  Serial.println(adcValue);
  Serial.print("Resistance: ");
  Serial.println(R_therm);
  Serial.print("Temperature (C): ");
  Serial.println(temperatureC);  
  return temperatureC;
}

String switch_state("off");
float cnt = 0.0;
void loop(void) {

  float temp = check_temp(PIN_TEMP1);

  bool send_data = true;
  if (send_data) {
    Serial.println("Initializing modem...");
    modem.begin();
    delay(10000);

    String modemInfo = modem.getModemInfo();
    Serial.print("Modem Info: ");
    Serial.println(modemInfo);

    if ( GSM_PIN && modem.getSimStatus() != 3 ) {
      modem.simUnlock(GSM_PIN);
    }

    Serial.print(String("Connect GPRS: apn: ") + apn + " user: \'" + gprsUser + "\' pw: \'" + gprsPass + "\' ...");
    modem.gprsConnect(apn, gprsUser, gprsPass);
    Serial.print("Waiting for network...");
    if (!modem.waitForNetwork()) {
      Serial.println(" fail");
      delay(10000);
      return;
    }
    Serial.println(" success");

    if (modem.isNetworkConnected()) {
      Serial.println("Network connected");
    }

    Serial.print(F("Connecting to \""));
    Serial.print(apn);
    Serial.print(F("\":"));
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      Serial.println(" failure");
      delay(10000);
      return;
    }
    Serial.println(" success");

    if (modem.isGprsConnected()) {
      Serial.println("GPRS connected");
    }
  }
  // changing resource each time to avoid hitting the cache
  // However, for consecutive attempts within the same iteration we want to hit the cache 
  // so that the response is faster
  String resource = String("/dev/res") + random(10000) + "?"
                            + "userId=463701923" 
                            + "&heating=" + switch_state
                            + "&sensorId=temp_1"
                            + "&value=" + temp;
  // If anything goes wrong with the connection, we want everything to be switched off
  switch_state = "off";
  
  if (send_data) {
    for (int retry = 0; retry < 3; retry++) {
      
      Serial.print(String(" server: ") + server + " port: " + port + " resource: " + resource + "\n");
      int httpResponseCode  = http.get(resource);
      http.endRequest();

      if (httpResponseCode != 0 ) {
        Serial.println(String("\n failed to connect. Error: ") + String(httpResponseCode));
        http.stop();
        delay(5000); // retrying
      }
      else {
        Serial.print("Response code: ");
        Serial.println(httpResponseCode);
        String body = http.responseBody();
        switch_state = parse_body(body);
        Serial.println("Found state: \"" + switch_state + "\"");
        http.stop();
        break;
      }
    }

    Serial.println(F("Server disconnected"));
    modem.gprsDisconnect();
    Serial.println(F("GPRS disconnected"));
  }
  if (switch_state.compareTo("on") == 0){
    digitalWrite(PIN_HEATING, HIGH);
  }
  else{
    digitalWrite(PIN_HEATING, LOW);
  }
  
  Serial.println("delay for a few minutes");
  delay(60000*5);
}
