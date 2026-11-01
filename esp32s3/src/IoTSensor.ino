#include <Arduino.h>
#include "driver/rtc_io.h"
#include "modem.h"
#include <SSLClient.h>
#include "ssl_certs_der.h"
#include <PubSubClient.h>
#include <DHT.h>

#define MQTT_BROKER    "a3fu7j5avgf87g-ats.iot.eu-west-3.amazonaws.com"
#define MQTT_PORT      8883
#define MQTT_CLIENT_ID "T-SIM7080G_01"
#define MQTT_TOPIC     "devices/SIM7080G_01/data"

// --- Timing (swap these for production) ---
#define SLEEP_INTERVAL_S   600         // dev: 60s  | production: 600
#define SEND_EVERY_N       6           // send every N readings

// --- Sensor reading ---
struct SensorReading {
    uint32_t ts;        // Unix timestamp (estimated, corrected at each send)
    float    temp_c;    // NAN if the sensor read failed
    float    humidity;  // NAN if the sensor read failed
    uint16_t batt_mv;   // Battery voltage in mV
    int8_t   batt_pct;  // Battery percentage 0–100
    bool     charging;
};

// --- Persistent state across deep sleeps ---
RTC_DATA_ATTR static SensorReading measurements[SEND_EVERY_N];
RTC_DATA_ATTR static int            measurement_count = 0;
RTC_DATA_ATTR static uint32_t       next_ts = 0;  // estimated Unix time of next reading, synced on each send

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

DHT dht(DHT_PIN, DHT11);

// --- Sensor reading ---
SensorReading read_sensors() {
    float temp_c = NAN;
    float humidity = NAN;
    // The DHT11 needs about 1s after power-up and 2s between reads, so retry rather than wait on every wake.
    for (int attempt = 0; attempt < 3; attempt++) {
        if (attempt) delay(2000);
        humidity = dht.readHumidity(true);
        temp_c = dht.readTemperature();
        if (!isnan(humidity) && !isnan(temp_c)) break;
    }
    return {
        next_ts,
        temp_c,
        humidity,
        (uint16_t)PMU.getBattVoltage(),
        (int8_t)PMU.getBatteryPercent(),
        PMU.isCharging(),
    };
}

// JSON has no NaN, so failed reads are sent as null.
void format_json_float(char* buf, size_t len, float value) {
    if (isnan(value)) snprintf(buf, len, "null");
    else snprintf(buf, len, "%.1f", value);
}

void on_config(char* topic, byte* payload, unsigned int len) {
    Serial.printf("[config] %.*s\n", (int)len, (char*)payload);
}

bool send_batch() {
    Serial.println("--- Send cycle start ---");

    Serial.println("[1/4] Powering modem...");
    PMU_setup(true);
    basic_modem_setup(modem);
    modem.sendAT(GF("+CSCLK=0")); modem.waitResponse();

    Serial.println("[2/4] Connecting to network...");
    if (!connect_GSM_1nce(modem)) return false;

    Serial.println("[3/4] Connecting MQTT...");
    // SSLClient validates certificates against the build time unless told otherwise,
    // which rejects any broker certificate issued after this firmware was compiled.
    uint32_t now = get_network_time(modem);
    if (now) {
        Serial.printf("Network time: %lu\n", now);
        sslClient.setVerificationTime(now / 86400 + 719528, now % 86400);  // days since year 0
        // The newest reading was taken just now, so shift the batch onto real time.
        uint32_t correction = now - next_ts;
        for (int i = 0; i < measurement_count; i++) measurements[i].ts += correction;
        next_ts = now;
    } else {
        Serial.println("Network time unavailable, using build time.");
    }
    sslClient.setMutualAuthParams(mTLS);
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setBufferSize(1024);
    mqtt.setKeepAlive(120);
    mqtt.setCallback(on_config);

    if (!mqtt.connect(MQTT_CLIENT_ID)) {
        Serial.printf("MQTT connect failed, rc=%d\n", mqtt.state());
        return false;
    }

    Serial.printf("[4/4] Publishing %d readings...\n", measurement_count);
    // Build JSON: {"readings":[{"ts":0,"temp":23.1,"hum":48.0,"batt_mv":3950,"batt_pct":82,"charging":false},...]}
    char payload[768];
    int pos = snprintf(payload, sizeof(payload), "{\"readings\":[");
    for (int i = 0; i < measurement_count; i++) {
        const SensorReading& r = measurements[i];
        char temp[8], hum[8];
        format_json_float(temp, sizeof(temp), r.temp_c);
        format_json_float(hum, sizeof(hum), r.humidity);
        pos += snprintf(payload + pos, sizeof(payload) - pos,
                        "%s{\"ts\":%lu,\"temp\":%s,\"hum\":%s,\"batt_mv\":%u,\"batt_pct\":%d,\"charging\":%s}",
                        i ? "," : "", r.ts, temp, hum,
                        r.batt_mv, r.batt_pct, r.charging ? "true" : "false");
    }
    snprintf(payload + pos, sizeof(payload) - pos, "]}");

    mqtt.subscribe("devices/SIM7080G_01/config");
    sslClient.flush();  // send SUBSCRIBE to broker immediately
    delay(1000);        // wait for SUBACK + retained PUBLISH over LTE
    mqtt.loop();        // process SUBACK (one SSL record)
    mqtt.loop();        // process retained PUBLISH → on_config fires

    bool ok = mqtt.publish(MQTT_TOPIC, payload);
    mqtt.loop();  // flush BearSSL buffer → AT+CASEND
    Serial.printf("Publish %s\n", ok ? "OK" : "FAILED");

    mqtt.disconnect();
    modem.gprsDisconnect();
    Serial.println("--- Send cycle done ---");
    return ok;
}

void setup() {
    Serial.begin(115200);
    dht.begin();
}

void loop() {
    PMU_setup(false);  // always init PMU first — needed for battery readings; modem enabled later if sending

    SensorReading r = read_sensors();
    measurements[measurement_count++] = r;
    Serial.printf("[reading %d/%d] ts=%lu temp=%.1f hum=%.1f batt=%dmV %d%%%s\n",
                  measurement_count, SEND_EVERY_N, r.ts, r.temp_c, r.humidity,
                  r.batt_mv, r.batt_pct, r.charging ? " charging" : "");

    if (measurement_count >= SEND_EVERY_N) {
        send_batch();
        measurement_count = 0;  // reset regardless of success to avoid getting stuck
        PMU.disableDC3();       // power down modem before sleep — PMU holds DC3 on independently
    }

    // When USB is connected: delay instead of deep sleep so the port stays visible for upload.
    // isPlugged() checks for USB frames from the host. Serial's bool can briefly read false while connected.
    // USB takes a moment to come back after a wake, so give the host time to appear before deciding to sleep.
    uint32_t usb_wait_start = millis();
    delay(200);
    while (!Serial.isPlugged() && millis() - usb_wait_start < 2000) delay(50);
    if (Serial.isPlugged()) {
        int usb_delay = 300 * 1000;
        Serial.printf("[sleep] USB detected — delaying %ds instead of sleeping.\n", usb_delay / 1000);
        next_ts += usb_delay / 1000;
        delay(usb_delay);
        return;
    }

    next_ts += SLEEP_INTERVAL_S;
    Serial.printf("[sleep] %ds. Next send in %d readings.\n",
                  SLEEP_INTERVAL_S, SEND_EVERY_N - measurement_count);
    Serial.flush();
    // BOOT button = GPIO0, active LOW. Enable RTC pull-up so the pin isn't floating during sleep.
    // The pull-up only holds if the RTC peripherals stay powered; otherwise the pin floats low and wakes us at once.
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    rtc_gpio_pullup_en(GPIO_NUM_0);
    rtc_gpio_pulldown_dis(GPIO_NUM_0);
    esp_sleep_enable_ext1_wakeup(1ULL << 0, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_INTERVAL_S * 1000000ULL);
    esp_deep_sleep_start();
}
