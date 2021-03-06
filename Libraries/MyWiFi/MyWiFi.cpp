
#include <Arduino.h>
#include "MyWiFi.h"

MyWiFi::MyWiFi(const char* ssid_, const char* password_){
	ssid = new char[strlen(ssid_)+1];
	strcpy(ssid, ssid_);
	password = new char[strlen(password_)+1];
	strcpy(password, password_);
}

MyWiFi::~MyWiFi(){
	delete[] ssid;
	delete[] password;
}

void MyWiFi::scanWiFiNetworks(){
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");
}


void MyWiFi::WiFiConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  int cnt = 0;
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED && cnt++ < 100) {
    delay(500);
    Serial.print(".");

    if (cnt % 10 == 0) {
      scanWiFiNetworks();
      WiFi.begin(ssid, password);
    }
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}
