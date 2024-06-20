#include <ESPmDNS.h>
#include "Utils.h"
#include "TempSensor.h"
#include <HX711.h>
#include <Preferences.h> // Using non-volatile storage to store individual values
Preferences preferences;

const int led = 13;
int extLED = 0;


const char server[] = "d2adilqhg7vm5d.cloudfront.net";
const char resource[] = "/dev/dynamodb?heating_switch=refresh&html=false";
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


void print_http_payload(HttpClient http) {
  unsigned long timeoutStart = millis();
  // Number of milliseconds to wait without receiving any data before we give up
  int kNetworkTimeout = 30*1000;
  // Number of milliseconds to wait if no data is available before trying again
  int kNetworkDelay = 1000;
  char c;
  // Whilst we haven't timed out & haven't reached the end of the body
  while ( (http.connected() || http.available()) &&
          (!http.endOfBodyReached()) &&
          ((millis() - timeoutStart) < kNetworkTimeout) )
  {
    if (http.available())
    {
      c = http.read();
      // Print out this character
      Serial.print(c);

      // We read something, reset the timeout counter
      timeoutStart = millis();
    }
    else
    {
      // We haven't got any data, so let's pause to allow some to
      // arrive
      delay(kNetworkDelay);
    }
  }
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

  Serial.begin(115200);
  SerialAT.begin(9600, SERIAL_8N1, MODEM_RX, MODEM_TX);
  modem.setBaud(9600);
}

int num = 0;
float cnt = 0.0;
void loop(void) {

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
    for (int retry = 0; retry < 3; retry++) {
      Serial.print(String(" server: ") + server + " port: " + port + " resource: " + resource + "\n");
      int httpResponseCode  = http.get(resource);

      if (httpResponseCode != 0 ) {
        Serial.println(String("\n failed to connect. Error: ") + String(httpResponseCode));
        http.stop();
        delay(5000); // retrying
      }
      else {
        Serial.print("Response code: ");
        Serial.println(httpResponseCode);
        Serial.print("Printing payload ");
        print_http_payload(http);
        http.stop();
        break;
      }
    }

    Serial.println(F("Server disconnected"));
    modem.gprsDisconnect();
    Serial.println(F("GPRS disconnected"));
  }
  Serial.println("delay for a few sec");
  delay(5000);
}
