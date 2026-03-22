#include <Arduino.h>
#include "setup.h"
#include "liligo_tools.h"
#include <SSLClient.h>
#include "ssl_certs_der.h"
#include <PubSubClient.h>

#define MQTT_BROKER    "a3fu7j5avgf87g-ats.iot.eu-west-3.amazonaws.com"
#define MQTT_PORT      8883
#define MQTT_CLIENT_ID "T-SIM7080G_01"
#define MQTT_TOPIC     "devices/SIM7080G_01/data"

TinyGsm modem(SerialAT);
TinyGsmClient tcpClient(modem, 0);  // plain TCP — SSL handled by BearSSL on ESP32
SSLClient sslClient(tcpClient, TAs, TAs_NUM, A0);
PubSubClient mqtt(sslClient);

int num_msg = 0;

void setup()
{
    Serial.begin(115200);

    PMU_setup();
    basic_modem_setup(modem);
    cmd_get_return(modem, "AT+CSCLK=0");

    connect_GSM_1nce(modem);

    // Set mutual TLS client cert + private key (BearSSL handles TLS on ESP32 side)
    SSLClientParameters mTLS = SSLClientParameters::fromDER(
        (const char*)client_cert_der, client_cert_der_len,
        (const char*)client_key_der, client_key_der_len
    );
    sslClient.setMutualAuthParams(mTLS);

    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setBufferSize(512);

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
    Serial.println("loop");
    mqtt.loop();

    if (!mqtt.connected()) {
        if (!reconnect()) {
            delay(10000);
            return;
        }
    }

    if (num_msg < 10) {
        char payload[64];
        snprintf(payload, sizeof(payload), "{\"temp\": 1.1111, \"msg\": %d}", num_msg);
        if (mqtt.publish(MQTT_TOPIC, payload)) {
            Serial.println("Publish successful!");
            num_msg++;
        } else {
            Serial.println("Publish failed.");
        }
    }
    delay(30000);
}
