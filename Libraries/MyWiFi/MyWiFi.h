#ifndef __MY_WIFI_H__
#define __MY_WIFI_H__

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>

class MyWiFi{
private:
	char* ssid;
	char* password;

public:
	MyWiFi(const char* ssid_, const char* password_);
	MyWiFi() = delete;
	~MyWiFi();
	void scanWiFiNetworks();
	void WiFiConnect();

};
#endif
