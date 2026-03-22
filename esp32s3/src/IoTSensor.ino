#include <Arduino.h>
#include "modem.h"
#include <SSLClient.h>
#include "ssl_certs_der.h"
#include <PubSubClient.h>

#define MQTT_BROKER    "a3fu7j5avgf87g-ats.iot.eu-west-3.amazonaws.com"
#define MQTT_PORT      8883
#define MQTT_CLIENT_ID "T-SIM7080G_01"
#define MQTT_TOPIC     "devices/SIM7080G_01/data"

// --- Timing (swap these for production) ---
#define SLEEP_INTERVAL_S   10          // dev: 10s  | production: 600
#define SEND_EVERY_N       6           // send every N readings (~1 min dev | ~1 hr production)

// --- Persistent state across deep sleeps ---
RTC_DATA_ATTR static float measurements[SEND_EVERY_N];
RTC_DATA_ATTR static int   measurement_count = 0;

// --- MQTT / SSL (re-initialised each send cycle) ---
TinyGsm modem(SerialAT);
TinyGsmClient tcpClient(modem, 0);
SSLClient sslClient(tcpClient, TAs, TAs_NUM, A0, 4096, SSLClient::SSL_WARN);
PubSubClient mqtt(sslClient);

// Must be global — sslClient holds a pointer into this object
SSLClientParameters mTLS = SSLClientParameters::fromDER(
    (const char*)client_cert_der, client_cert_der_len,
    (const char*)client_key_der, client_key_der_len
);

// --- Fake sensor — replace with real read ---
float read_sensor() {
    return 20.0f + (esp_random() % 100) * 0.1f;
}

void go_to_sleep() {
    Serial.printf("Sleeping for %ds. Next send in %d readings.\n",
                  SLEEP_INTERVAL_S, SEND_EVERY_N - measurement_count);
    Serial.flush();
    esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_INTERVAL_S * 1000000ULL);
    esp_deep_sleep_start();
}

bool send_batch() {
    PMU_setup(true);
    basic_modem_setup(modem);
    modem.sendAT(GF("+CSCLK=0")); modem.waitResponse();

    if (!connect_GSM_1nce(modem)) return false;

    sslClient.setMutualAuthParams(mTLS);
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setBufferSize(512);
    mqtt.setKeepAlive(120);

    Serial.println("Connecting MQTT...");
    if (!mqtt.connect(MQTT_CLIENT_ID)) {
        Serial.printf("MQTT connect failed, rc=%d\n", mqtt.state());
        return false;
    }
    Serial.println("MQTT connected.");

    // Build JSON: {"readings":[20.1,21.3,...]}
    char payload[128];
    int pos = snprintf(payload, sizeof(payload), "{\"readings\":[");
    for (int i = 0; i < measurement_count; i++) {
        pos += snprintf(payload + pos, sizeof(payload) - pos,
                        i ? ",%.1f" : "%.1f", measurements[i]);
    }
    snprintf(payload + pos, sizeof(payload) - pos, "]}");

    bool ok = mqtt.publish(MQTT_TOPIC, payload);
    mqtt.loop();  // flush BearSSL buffer → AT+CASEND
    Serial.printf("Publish %s: %s\n", ok ? "OK" : "FAILED", payload);

    mqtt.disconnect();
    modem.gprsDisconnect();
    return ok;
}

void setup() {
    Serial.begin(115200);

    // Collect one reading
    float sample = read_sensor();
    measurements[measurement_count++] = sample;
    Serial.printf("Reading %d/%d: %.1f\n", measurement_count, SEND_EVERY_N, sample);

    if (measurement_count >= SEND_EVERY_N) {
        PMU_setup(true);
        send_batch();
        measurement_count = 0;  // reset regardless of success to avoid getting stuck
    } else {
        PMU_setup(false);  // modem not needed — keep it off to save power
    }

    go_to_sleep();
}

void loop() {
    // Never reached — device always deep sleeps from setup()
}
