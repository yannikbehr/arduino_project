#include <ESPmDNS.h>
#include "Utils.h"
#include "TempSensor.h"
#include <HX711.h>

const int led = 13;
int extLED = 0;


//const char server[] = "d29s56othfawu8.cloudfront.net";
const char server[] = "d2m3mgw5dldwts.cloudfront.net";
//const char server[] = "example.com";
const char resourceWeight[] = "/dev/DyDB/WeightKg/";
const char resourceTemp[] = "/dev/DyDB/TempC/";
//const char resource[] = "/";
const int port = 80 ;

// Modemsetup and pins
#define TINY_GSM_MODEM_SIM800
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26

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
// Lebara SIM: Switzerland
//const char apn[]      = "internet.eplus.de"; // APN (example: internet.vodafone.pt) use https://wiki.apnchanger.org
//const char gprsUser[] = "eplus"; // GPRS User
//const char gprsPass[] = "gprs"; // GPRS Password
// Lebara SIM: Italy
//const char apn[]      = "internet";
//const char gprsUser[] = ""; // GPRS User
//const char gprsPass[] = ""; // GPRS Password

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

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_UNDEFINED       : Serial.println("Wakeup caused by UNDEF    "); break;
    case ESP_SLEEP_WAKEUP_ALL             : Serial.println("Wakeup caused by ALL      "); break;
    case ESP_SLEEP_WAKEUP_EXT0            : Serial.println("Wakeup caused by EXT0     "); break;
    case ESP_SLEEP_WAKEUP_EXT1            : Serial.println("Wakeup caused by EXT1     "); break;
    case ESP_SLEEP_WAKEUP_TIMER           : Serial.println("Wakeup caused by TIMER    "); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD        : Serial.println("Wakeup caused by TOUCHPAD "); break;
    case ESP_SLEEP_WAKEUP_ULP             : Serial.println("Wakeup caused by ULP      "); break;
    case ESP_SLEEP_WAKEUP_GPIO            : Serial.println("Wakeup caused by GPIO     "); break;
    case ESP_SLEEP_WAKEUP_UART            : Serial.println("Wakeup caused by UART     "); break;
    //case ESP_SLEEP_WAKEUP_WIFI            : Serial.println("Wakeup caused by WIFI     "); break;
    //case ESP_SLEEP_WAKEUP_COCPU           : Serial.println("Wakeup caused by COCPU    "); break;
    //case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG : Serial.println("Wakeup caused by COCPU_TT "); break;
    //case ESP_SLEEP_WAKEUP_BT              : Serial.println("Wakeup caused by BT       "); break;
    default : Serial.println("Wakeup was not caused by deep sleep"); break;
  }
}

void print_wakeup_touchpad() {
  touch_pad_t touchPin;

  touchPin = esp_sleep_get_touchpad_wakeup_status();

  switch (touchPin)
  {
    case 0  : Serial.println("Touch detected on GPIO 4"); break;
    case 1  : Serial.println("Touch detected on GPIO 0"); break;
    case 2  : Serial.println("Touch detected on GPIO 2"); break;
    case 3  : Serial.println("Touch detected on GPIO 15"); break;
    case 4  : Serial.println("Touch detected on GPIO 13"); break;
    case 5  : Serial.println("Touch detected on GPIO 12"); break;
    case 6  : Serial.println("Touch detected on GPIO 14"); break;
    case 7  : Serial.println("Touch detected on GPIO 27"); break;
    case 8  : Serial.println("Touch detected on GPIO 33"); break;
    case 9  : Serial.println("Touch detected on GPIO 32"); break;
    default : Serial.println("Wakeup not by touchpad"); break;
  }
}

// waegezelle
HX711 wz;
int pin1 = 33;
int pin2 = 25;
//int pin1 = 25;
//int pin2 = 33;

int tmpPin = 35;
void setup(void) {
  pinMode(pin1, INPUT);
  pinMode(pin2, INPUT);
  Serial.begin(115200);
  delay(1000);
  print_wakeup_reason();
  print_wakeup_touchpad();
  wz.begin(pin1, pin2);
  wz.wait_ready();
  bool tara = false;
  if (tara) {
    Serial.println("Now doing tara ... ");
    wz.tare(20);
    for (int i = 0; i < 10; i++) {
      my_print("Tara in %i seconds\n", 10 - i);
      delay(1000);
    }
    //wz.set_scale(1.0);
    //wz.set_offset(0.0);
    Serial.println("Now put the weight... ");
    delay(10000);
    for (int i=0;i<10;i++){
      my_print("Raw values from wz: ");
      Serial.print(wz.get_value(5));
      Serial.print(", ");
      Serial.println(wz.get_units(5));
      delay(500);
    }
    
    Serial.println("Now calibrating ... ");
    wz.callibrate_scale(11200, 20);
    Serial.println("Done");

    float scale = wz.get_scale();
    long  offset = wz.get_offset();
    Serial.print("Scale: ");
    Serial.print(scale);
    Serial.print(" offset: ");
    Serial.println(offset);
  }
  //wz.set_offset(8333370);
  //wz.set_scale(-13.70);
  //wz.set_offset(110152);
  //wz.set_scale(13.60);
  //wz.set_scale(-17.90); // see evernote Stockwaage
  //wz.set_offset(-8454);
  wz.set_scale(-27.76);
  wz.set_offset(-113026);


  //pinMode(led, OUTPUT);
  pinMode(extLED, OUTPUT);
  pinMode(tmpPin, INPUT);

  //digitalWrite(led, 0);
  digitalWrite(extLED, 0);

  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  // Modem setup
  // Set your reset, enable, power pins here
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);


  SerialAT.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX);
  modem.setBaud(9600);

  // DeepSleep settings
#define uS_TO_S_FACTOR 1000000  // convert to micro seconds
  uint64_t sleep_s = 3600*0.5; // time to sleep in seconds
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  //esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TOUCHPAD);
  //esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_timer_wakeup(sleep_s * uS_TO_S_FACTOR);
  Serial.println(String("Setup ESP32 to sleep for every ") + String((int) sleep_s) +
                 " Seconds");
}

int num = 0;
float cnt = 0.0;
void loop(void) {

  // measure before connecting to GSM otherwise the modem might take that much energy that
  // it can affect measurements
  
  
  int num_measurements = 20;
  int measurements[num_measurements];
  for (int i=0; i<num_measurements; i++){
    wz.wait_ready();
    measurements[i] = wz.get_units(20);
    Serial.println(String("wz: ") + measurements[i]);
    //if (j>0)
    //  break;
    delay(500);
  }
  int j = median(measurements, num_measurements);

  Serial.print("post to DyDB: ");
  Serial.println(j);
  delay(1000);

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

    Serial.print(F("Connect GPRS ......."));
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
    for (int retry=0; retry<3; retry++){
      // post to DyDB
      Serial.print(String("Performing HTTP GET request: retry:") + String(retry));
      String resource_cmp = String(resourceWeight) + String(j);
      Serial.print(String(" server: ") + server + " port: " + port + " resource: " + resource_cmp + " ... value:" + String(j) + "\n");
      int http_err1 = http.get(resource_cmp);
      http.stop();

      resource_cmp = String(resourceTemp) + String(20.0);
      Serial.print(String(" server: ") + server + " port: " + port + " resource: " + resource_cmp + "\n");
      int http_err2 = http.get(resource_cmp);
      http.stop();
      
      if (http_err1 != 0 || http_err2 != 0) {
        Serial.println(String("\n failed to connect. Error: ") + String(http_err1) + " Tmp: " + String(http_err2));
        delay(5000); // retrying
        //return;
      }
      else {
        Serial.println(String(" ... Success \n"));
        break;
      }
    }
    
    Serial.println(F("Server disconnected"));
    modem.gprsDisconnect();
    Serial.println(F("GPRS disconnected"));
  }

  Serial.println("Going to sleep now");
  delay(1000);
  Serial.flush();
  esp_deep_sleep_start();

  //}
  //else {
  // delay(1000);
  //}
}
