#include <ESPmDNS.h>
#include "Utils.h"
#include "TempSensor.h"
#include <HX711.h>

#include "MyWebServer.h"
MyWebServer WebS;

// all the wifi stetup
#include "MyWiFi.h"
const char* ssid = "FRITZ!Box 7530 GT";
const char* password = "28795620229912961788";
MyWiFi wifi(ssid, password);

const int led = 13;
int extLED = 0;
bool postToDyDB = true;

// http client functionality
//#include <HTTPClient.h>
#include "DyDBConnect.h"
DyDBConnect DyDB(String("https://t0unry1mdj.execute-api.eu-west-3.amazonaws.com/test1"));

// waegezelle
HX711 wz;
int pin1 = 33;
int pin2 = 25;

int tmpPin = 35;
void setup(void) {
  pinMode(pin1, INPUT);
  pinMode(pin2, INPUT);
  Serial.begin(115200);
  wz.begin(pin1, pin2);
  wz.wait_ready();
  Serial.println("Now doing tara ... ");
  //wz.tare(20);
  //for (int i=0; i<10; i++){
  //    my_print("Tara in %i seconds\n", 10-i);
  //    delay(1000);
  //}
  //Serial.println("Now put the weight... ");
  //delay(20000);
  //Serial.println("Now calibrating ... ");
  //wz.callibrate_scale(2012, 20);
  //Serial.println("Done");

  //float scale = wz.get_scale();
  //long  offset = wz.get_offset();
  //my_print("Scale: %f, offset:%i \n", scale, offset);
  wz.set_offset(8333370);
  wz.set_scale(-13.70);
  
  pinMode(led, OUTPUT);
  pinMode(extLED, OUTPUT);
  pinMode(tmpPin, INPUT);

  digitalWrite(led, 0);
  digitalWrite(extLED, 0);
  Serial.begin(115200);

  wifi.WiFiConnect();

  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  WebS.API_setup();
  WebS.begin();

  Serial.println("HTTP server started");
}

int num=0;
float cnt=0.0;
void loop(void) {
  //WebS.handleClient();
  
  wz.wait_ready();
  int j = wz.get_units(20);
  
  Serial.print("post to DyDB: ");
  Serial.println(j);
  DyDB.get(String("/DyDB/WeightKg/"), j);

  delay(10);

  int uS_TO_S_FACTOR = 1000000; // comvert to micro seconds
  int sleep_s = 300; // time to sleep in seconds
  esp_sleep_enable_timer_wakeup(sleep_s * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(sleep_s) +
  " Seconds");
  esp_deep_sleep_start();
}
