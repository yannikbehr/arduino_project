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

// Pin heating
#define PIN_HEATING          15

// Temp pins
#define PIN_TEMP1            0
#define PIN_TEMP2            2


// Constants for temp for tru-components temp sensor with 10 kΩ 3950 K
const float VCC = 3.3;         // Supply voltage
const float R_FIXED = 100;   // Fixed resistor value (100Ω)

//const float TEMP0 = 298.15;// Reference temperature (25°C in Kelvin)
const float TEMP0 = 273.15;  // Reference temperature (0ºC in Kelvin)
//const float BETA = 3850;   // Beta parameter
const float BETA = 265.24;   // estimated with two temp points
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

int parse_body(String body) {
  if (strstr(body.c_str(), "switch:on")) {
    return 1;
  }
  if (strstr(body.c_str(), "switch:off")) {
    return 0;
  }
  return -1;
}
// time to sleep in seconds
uint64_t sleep_s = 60 * 30;

int counter;
// Switch for the heating, on:1 off:0 or unclear:-1
int state;
void setup(void) {
  // Non-volatile storage...
  // create reboot counter
  preferences.begin("my-app", false);
  counter = preferences.getUInt("counter", 0);
  preferences.putUInt("counter", ++counter);
  state = preferences.getUInt("state", 0); // default off
  preferences.end();

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
  pinMode(PIN_TEMP2, INPUT);

  Serial.begin(115200);
  SerialAT.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX);
  modem.setBaud(9600);

  // DeepSleep settings
#define uS_TO_S_FACTOR 1000000  // convert to micro seconds
  esp_sleep_enable_timer_wakeup(sleep_s * uS_TO_S_FACTOR);
  Serial.println(String("Setup ESP32 to sleep for every ") + String((int) sleep_s) +
                 " Seconds");
}

int send_data_get_state(int state, float temp, float temp2) {

  // If anything goes wrong with the connection, we want everything to be switched off
  int ret = 0;

  String switch_state = "unclear";
  if (state == 1) {
    switch_state = "on";
  }
  else if (state == 0) {
    switch_state = "off";
  }

  // changing resource each time to avoid hitting the cache
  // However, for consecutive attempts within the same iteration we want to hit the cache
  // so that the response is faster
  String resource = String("/dev/res") + random(10000) + "?"
                    + "userId=463701923"
                    + "&heating=" + switch_state
                    + "&tempSensor1=" + temp
                    + "&tempSensor2=" + temp2;

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
      ret = parse_body(body);
      Serial.println("Found state: " + String(ret));
      http.stop();
      break;
    }
  }
  return ret;
}

// polynomial fit parameters estimated with
// git/arduino_project/hone/temp_sensors/tmp.py
float analog_read_avg(int pin, int sensor_id) {
  float z1[4] = {-5.32885082e+03,  3.45109944e+01, -7.45169535e-02,  5.37018932e-05}; 
  float z2[4] = {-2.76016582e+00,  2.02217144e-02, -9.62514519e-07,  4.54582506e-10}; 
  int split_val = 487;
  int num = 20;
  int measurements[num];
  for (int i = 0; i < num; i++) {
    measurements[i] = analogRead(pin);
    delay(100);
  }
  int val = median(measurements, num);
  if (val < split_val){
    return z1[0] + val*z1[1] + val*val*z1[2] + val*val*val*z1[3];
  }
  return z2[0] + val*z2[1] + val*val*z2[2] + val*val*val*z2[3];
}

float cnt = 0.0;
void loop(void) {

  // sleep before checking temp to make sure that everything is in a steady state
  delay(2000);
  // calculating average over a couple of measurements 
  float temp = 0; 
  float temp2 = 0;
  if (false){
    int num = 10;
    for (int i=0; i<num; i++){
      float val1 = analog_read_avg(PIN_TEMP1, 1);
      float val2 = analog_read_avg(PIN_TEMP2, 2);
      Serial.println("(" + String(val1) + ", " + String(val2) + ")");
      temp += val1;
      temp2 += val2;
      delay(1000);
    }
    temp /= num;
    temp2 /= num;
  }
  
  if (true) {
    // This output is used to gather some data to fit the temperatures
    for (int i = 0; i < 100; i++) {
      //float adcValue1 = analog_read_avg(PIN_TEMP1, 1);
      int adcValue1 = analogRead(PIN_TEMP1);
      int adcValue2 = analogRead(PIN_TEMP2);
      Serial.print("(");
      Serial.print(adcValue1);
      Serial.print(",");
      Serial.print(adcValue2);
      Serial.print("), ");
      if (i % 10 == 0){
          Serial.println("");
      }
      delay(50);
    }
    Serial.println();
    delay(5000);
    return;
  }
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

  if (send_data) {
    state = send_data_get_state(state, temp, temp2);

    Serial.println(F("Server disconnected"));
    modem.gprsDisconnect();
    Serial.println(F("GPRS disconnected"));
    Serial.flush();    
  }
  // store state to persistent memory
  if (state == 0 || state == 1) {
    preferences.begin("my-app", false);
    preferences.putUInt("state", state);
    preferences.end();
  }

  if (state == 1) {
    digitalWrite(PIN_HEATING, HIGH);
    // not going to deep sleep if heating is on
    delay(sleep_s * 1000);
  }
  else {
    digitalWrite(PIN_HEATING, LOW);
    Serial.println("Going to sleep now");
    esp_deep_sleep_start();
  }
}
