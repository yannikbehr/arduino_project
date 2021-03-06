#include <arduino.h>
#include "MyWebServer.h"

WebServer MyServer(80);
int MySwitch1;

int MyWebServer::getSwitchVal(){
	return MySwitch1;
}

void MyWebServer::handleClient(){
	MyServer.handleClient();
}

void MyWebServer::handleRoot() {
  MyServer.send(200, "text/plain", "hello from esp8266!");
}

void MyWebServer::begin(){
	MyServer.begin();
}

void MyWebServer::handleNotFound() {
  String message = "Endpoint Not Found\n\n";
  message += "URI: ";
  message += MyServer.uri();
  message += "\nMethod: ";
  message += (MyServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += MyServer.args();
  message += "\n";
  for (uint8_t i = 0; i < MyServer.args(); i++) {
    message += " " + MyServer.argName(i) + ": " + MyServer.arg(i) + "\n";
  }
  MyServer.send(404, "text/plain", message);
}

void MyWebServer::API_setup() {

  // Endpoint "/" handle
  //MyServer.on("/", handleRoot);

  // Endpoint "/on" handle  (function definition in place)
  MyServer.on("/on", []() {
    MySwitch1 = 1;
    MyServer.send(200, "text/plain", "switch1 is on");
  });

  // Endpoint "/off" handle
  MyServer.on("/off", []() {
    MySwitch1 = 0;
    MyServer.send(200, "text/plain", "switch1 is off");
  });

  //MyServer.onNotFound(handleNotFound);
}


