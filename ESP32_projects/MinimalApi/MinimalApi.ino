#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

const char* ssid = "FRITZ!Box 7530 GT";
const char* password = "28795620229912961788";

WebServer server(80);

const int led = 13;
int extLED = 0;
bool postToDyDB = false;

void handleRoot() {
  digitalWrite(led, 1);
  server.send(200, "text/plain", "hello from esp8266!");
  digitalWrite(led, 0);
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "Endpoint Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void scanWiFiNetworks() {

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

void WiFiConnect() {

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

void API_setup() {

  // Endpoint "/" handle
  server.on("/", handleRoot);

  // Endpoint "/on" handle
  server.on("/on", []() {
    digitalWrite(led, 1);
    digitalWrite(extLED, 1);
    postToDyDB = true;
    server.send(200, "text/plain", "LED is on");
  });

  // Endpoint "/off" handle
  server.on("/off", []() {
    digitalWrite(led, 0);
    digitalWrite(extLED, 0);
    postToDyDB = false;
    server.send(200, "text/plain", "LED is off");
  });

  // Endpoint "/inline" handle
  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });

  server.on("/connect",  httpClientConnect);

  server.onNotFound(handleNotFound);
}

// http client functionality
#include <HTTPClient.h>

void postMessage(float valueToPost) {
  HTTPClient http;
  String MyURL = "https://td5hgmj3n8.execute-api.eu-central-1.amazonaws.com/test2/DyDB/" + String(valueToPost);
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


void httpClientConnect() {

  Serial.print("[HTTP] begin...\n");
  // configure traged server and url
  //http.begin("http://example.com/index.html"); //HTTP
  postMessage(1.732);

  server.send(200, "text/plain", "Message posted");
}

int tmpPin = 35;
void setup(void) {
  pinMode(led, OUTPUT);
  pinMode(extLED, OUTPUT);
  pinMode(tmpPin, INPUT);

  digitalWrite(led, 0);
  digitalWrite(extLED, 0);
  Serial.begin(115200);

  WiFiConnect();

  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  API_setup();

  server.begin();
  Serial.println("HTTP server started");
}

int num=0;
float cnt=0.0;
void loop(void) {
  server.handleClient();

  int i=analogRead(tmpPin);
  if (num % 1000 == 0){
    Serial.print("analogRead: ");
    Serial.println(i);
  }
  if (num++ %50000 ==0 && postToDyDB){
    //cnt+=1.111111111;
    cnt = i;
    postMessage(cnt);
  }
  delay(10);
}
