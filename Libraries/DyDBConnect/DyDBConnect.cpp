#include <Arduino.h>
#include "DyDBConnect.h"
#include <HTTPClient.h>

DyDBConnect::DyDBConnect(String url_){
	baseUrl = url_;
}

void DyDBConnect::get(String endpoint, float val){

  HTTPClient http;
  String MyURL = baseUrl + endpoint + String(val);
  http.begin(MyURL);

  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();

  // httpCode will be negative on error
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();	
}
