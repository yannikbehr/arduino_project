#include <Arduino.h>
#include "modem.h"
#include <SSLClient.h>
#include "ssl_certs_der.h"
#include <PubSubClient.h>

#define MQTT_BROKER    "a3fu7j5avgf87g-ats.iot.eu-west-3.amazonaws.com"
#define MQTT_PORT      8883
#define MQTT_CLIENT_ID "T-SIM7080G_01"
#define MQTT_TOPIC     "devices/SIM7080G_01/data"

TinyGsm modem(SerialAT);
TinyGsmClient tcpClient(modem, 0);  // plain TCP — SSL handled by BearSSL on ESP32
SSLClient sslClient(tcpClient, TAs, TAs_NUM, A0, 4096, SSLClient::SSL_WARN);
PubSubClient mqtt(sslClient);

// Must be global — sslClient holds a pointer into this object
SSLClientParameters mTLS = SSLClientParameters::fromDER(
    (const char*)client_cert_der, client_cert_der_len,
    (const char*)client_key_der, client_key_der_len
);

int num_msg = 0;
unsigned long lastPublishTime = 0;

void setup()
{
    Serial.begin(115200);

    PMU_setup();
    basic_modem_setup(modem);
    cmd_get_return(modem, "AT+CSCLK=0");

    connect_GSM_1nce(modem);

    sslClient.setMutualAuthParams(mTLS);

    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setBufferSize(512);
    mqtt.setKeepAlive(120);  // AWS timeout = 120*1.5 = 180s; prevents MQTT_KEEP_ALIVE_TIMEOUT

    Serial.println("Setup complete. Entering loop.");
}

bool reconnect() {
    Serial.println("Connecting MQTT (SSLClient/BearSSL)...");
    if (mqtt.connect(MQTT_CLIENT_ID)) {
        Serial.println("MQTT connected!");
        Serial.println("OK -----------\n\n");
        return true;
    }
    Serial.printf("MQTT connect failed, rc=%d\n", mqtt.state());
    Serial.println("FAILED -----------\n\n");
    return false;
}

void loop()
{
    mqtt.loop();

    if (!mqtt.connected()) {
        if (!reconnect()) {
            delay(5000);
            return;
        }
    }

    unsigned long now = millis();
    if (num_msg < 10 && now - lastPublishTime >= 30000) {
        char payload[64];
        snprintf(payload, sizeof(payload), "{\"temp\": 1.1111, \"msg\": %d}", num_msg);
        if (mqtt.publish(MQTT_TOPIC, payload)) {
            // Call mqtt.loop() to flush BearSSL's buffered data (triggers AT+CASEND)
            mqtt.loop();
            Serial.println("Publish successful!");
            num_msg++;
        } else {
            Serial.println("Publish failed.");
        }
        lastPublishTime = now;
    }

    delay(100);
}
