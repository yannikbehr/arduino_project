#include <Arduino.h>
#include "driver/rtc_io.h"
#include "modem.h"
#include <SSLClient.h>
#include "ssl_certs_der.h"
#include <PubSubClient.h>

#define MQTT_BROKER    "a3fu7j5avgf87g-ats.iot.eu-west-3.amazonaws.com"
#define MQTT_PORT      8883
#define MQTT_CLIENT_ID "T-SIM7080G_01"
#define MQTT_TOPIC     "devices/SIM7080G_01/data"

// --- Timing (swap these for production) ---
#define SLEEP_INTERVAL_S   60         // dev: 10s  | production: 600
#define SEND_EVERY_N       3           // send every N readings 

// --- Sensor reading ---
struct SensorReading {
    uint32_t ts;        // Unix timestamp (estimated)
    float    t1;
    float    t2;
    uint16_t batt_mv;   // Battery voltage in mV
    int8_t   batt_pct;  // Battery percentage 0–100
    bool     charging;
};

// --- Persistent state across deep sleeps ---
RTC_DATA_ATTR static SensorReading measurements[SEND_EVERY_N];
RTC_DATA_ATTR static int            measurement_count = 0;
RTC_DATA_ATTR static uint32_t       next_ts = 0;  // estimated Unix time of next reading
                                                   // TODO: sync from modem AT+CCLK on connect

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

// --- Fake sensors — replace with real reads ---
SensorReading read_sensors() {
    return {
        next_ts,
        20.0f + (esp_random() % 100) * 0.1f,  // t1
        15.0f + (esp_random() % 100) * 0.1f,  // t2
        (uint16_t)PMU.getBattVoltage(),
        (int8_t)PMU.getBatteryPercent(),
        PMU.isCharging(),
    };
}

void go_to_sleep() {
    next_ts += SLEEP_INTERVAL_S;

    // If USB is connected, stay awake so the port remains visible for firmware upload.
    // Unplug USB to resume normal deep-sleep operation.
    delay(200);  // allow USB to enumerate
    if (Serial) {
        Serial.println("USB connected — staying awake for upload. Unplug to resume.");
        while (true) delay(1000);
    }

    Serial.printf("Sleeping for %ds. Next send in %d readings.\n",
                  SLEEP_INTERVAL_S, SEND_EVERY_N - measurement_count);
    Serial.flush();
    esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_INTERVAL_S * 1000000ULL);
    // BOOT button = GPIO0, active LOW. Enable RTC pull-up so the pin isn't floating during sleep.
    rtc_gpio_pullup_en(GPIO_NUM_0);
    rtc_gpio_pulldown_dis(GPIO_NUM_0);
    esp_sleep_enable_ext1_wakeup(1ULL << 0, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();
}

bool send_batch() {
    PMU_setup(true);
    basic_modem_setup(modem);
    modem.sendAT(GF("+CSCLK=0")); modem.waitResponse();

    if (!connect_GSM_1nce(modem)) return false;

    sslClient.setMutualAuthParams(mTLS);
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setBufferSize(1024);
    mqtt.setKeepAlive(120);

    Serial.println("Connecting MQTT...");
    if (!mqtt.connect(MQTT_CLIENT_ID)) {
        Serial.printf("MQTT connect failed, rc=%d\n", mqtt.state());
        return false;
    }
    Serial.println("MQTT connected.");

    // Build JSON: {"readings":[{"ts":0,"t1":23.1,"t2":18.4,"batt_mv":3950,"batt_pct":82,"charging":false},...]}
    char payload[512];
    int pos = snprintf(payload, sizeof(payload), "{\"readings\":[");
    for (int i = 0; i < measurement_count; i++) {
        const SensorReading& r = measurements[i];
        pos += snprintf(payload + pos, sizeof(payload) - pos,
                        "%s{\"ts\":%lu,\"t1\":%.1f,\"t2\":%.1f,\"batt_mv\":%u,\"batt_pct\":%d,\"charging\":%s}",
                        i ? "," : "", r.ts, r.t1, r.t2,
                        r.batt_mv, r.batt_pct, r.charging ? "true" : "false");
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
    PMU_setup(false);  // always init PMU first — needed for battery readings; modem enabled later if sending

    SensorReading r = read_sensors();
    measurements[measurement_count++] = r;
    Serial.printf("Reading %d/%d: ts=%lu t1=%.1f t2=%.1f batt=%dmV %d%% %s\n",
                  measurement_count, SEND_EVERY_N, r.ts, r.t1, r.t2,
                  r.batt_mv, r.batt_pct, r.charging ? "charging" : "");

    if (measurement_count >= SEND_EVERY_N) {
        send_batch();
        measurement_count = 0;  // reset regardless of success to avoid getting stuck
        PMU.disableDC3();       // power down modem before sleep — PMU holds DC3 on independently
    }

    go_to_sleep();
}

void loop() {
    // Never reached — device always deep sleeps from setup()
}
