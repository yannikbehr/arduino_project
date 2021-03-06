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
bool postToDyDB = false;

// http client functionality
//#include <HTTPClient.h>
#include "DyDBConnect.h"
DyDBConnect DyDB(String("https://td5hgmj3n8.execute-api.eu-central-1.amazonaws.com/test2"));

//void httpClientConnect() {
//
//  Serial.print("[HTTP] begin...\n");
//  // configure traged server and url
//  //http.begin("http://example.com/index.html"); //HTTP
//  postMessage(1.732);
//
//  server.send(200, "text/plain", "Message posted");
//}

int tmpPin = 35;
void setup(void) {
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
  WebS.handleClient();
  
  int i=analogRead(tmpPin);
  if (num % 1000 == 0){
    Serial.print("analogRead: ");
    Serial.println(i);
  }
  if (num++ %50000 ==0 && postToDyDB){
    //cnt+=1.111111111;
    cnt = i;
    DyDB.get(String("/DyDB/"), cnt);
  }
  delay(10);
}
