#define TINY_GSM_MODEM_SIM7080
#define TINY_GSM_RX_BUFFER 1024

#include <TinyGsmClient.h>

// LilyGo T-SIM7080G-S3 Pin Definitions
#define UART_BAUD   115200
#define PIN_DTR     27
#define PIN_TX      17
#define PIN_RX      18
#define PWR_PIN     41

// 1NCE Credentials
const char apn[] = "iot.1nce.net";

HardwareSerial modemSerial(1);
TinyGsm modem(modemSerial);

void setup() {
  Serial.begin(115200);
  delay(1000);

  // 1. Power on the modem
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, LOW);
  delay(100);
  digitalWrite(PWR_PIN, HIGH);
  delay(1000);
  digitalWrite(PWR_PIN, LOW);

  Serial.println("Initializing modem...");
  modemSerial.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

  // 2. Wait for modem to respond
  if (!modem.restart()) {
    Serial.println("Failed to restart modem. Check power/wiring.");
    while (true);
  }

  // 3. Set Network Mode to CAT-M (Preferred for 1NCE in CH/IT)
  modem.sendAT("+CMNB=1"); 
  modem.waitResponse();

  Serial.print("Connecting to APN: ");
  Serial.println(apn);

  if (!modem.gprsConnect(apn)) {
    Serial.println("Connection failed. Is the antenna connected?");
    while (true);
  }

  Serial.println("SUCCESS! Connected to the network.");
  Serial.print("Signal Quality: ");
  Serial.println(modem.getSignalQuality());
  
  // Get IP
  String ip = modem.getLocalIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
}

void loop() {
  Serial.print("In loop");
  delay(1000);
}
