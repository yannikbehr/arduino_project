#include <Arduino.h>
#include "driver/rtc_io.h"
#include "modem.h"
#include <SSLClient.h>
#include "ssl_certs_der.h"
#include <PubSubClient.h>
#include <DHT.h>
#include <Preferences.h>

#define MQTT_BROKER    "a3fu7j5avgf87g-ats.iot.eu-west-3.amazonaws.com"
#define MQTT_PORT      8883
#define MQTT_CLIENT_ID "T-SIM7080G_01"
#define MQTT_TOPIC     "devices/SIM7080G_01/data"
#define MQTT_CONFIG_TOPIC "devices/SIM7080G_01/config"

// --- Timing (swap these for production) ---
#define SLEEP_INTERVAL_S   600         // default; overridden by "sleep_s" in the config topic
#define SLEEP_MIN_S        30          // accepted range for "sleep_s": a too-long sleep would lose remote control
#define SLEEP_MAX_S        7200
#define SEND_EVERY_N       6           // send every N readings

// --- Sensor reading ---
struct SensorReading {
    uint32_t ts;        // Unix timestamp (estimated, corrected at each send)
    float    temp_c;    // NAN if the sensor read failed
    float    humidity;  // NAN if the sensor read failed
    uint16_t batt_mv;   // Battery voltage in mV
    int8_t   batt_pct;  // Battery percentage 0–100
    bool     charging;
    bool     vbus;      // external power (USB or solar input) present
    bool     relay;     // relay state at the time of the reading
};

// --- Persistent state across deep sleeps ---
RTC_DATA_ATTR static SensorReading measurements[SEND_EVERY_N];
RTC_DATA_ATTR static int            measurement_count = 0;
RTC_DATA_ATTR static uint32_t       next_ts = 0;  // estimated Unix time of next reading, synced on each send

// Deep sleep between readings. Kept in NVS flash so it survives a power cut.
static uint32_t sleep_s = SLEEP_INTERVAL_S;
Preferences prefs;

void load_settings() {
    prefs.begin("settings", true);
    uint32_t stored = prefs.getUInt("sleep_s", SLEEP_INTERVAL_S);
    prefs.end();
    if (stored >= SLEEP_MIN_S && stored <= SLEEP_MAX_S) sleep_s = stored;
    Serial.printf("[settings] sleep_s=%lu\n", sleep_s);
}

void set_sleep_s(long value) {
    if (value < SLEEP_MIN_S || value > SLEEP_MAX_S) {
        Serial.printf("[config] sleep_s=%ld rejected, must be %d..%d, keeping %lu\n",
                      value, SLEEP_MIN_S, SLEEP_MAX_S, sleep_s);
        return;
    }
    if ((uint32_t)value == sleep_s) return;  // retained config arrives on every connect, so only write changes
    sleep_s = value;
    prefs.begin("settings", false);
    prefs.putUInt("sleep_s", sleep_s);
    prefs.end();
    Serial.printf("[config] sleep_s=%lu saved\n", sleep_s);
}

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

// Not kept across deep sleep: every wake starts with the relay off.
static bool relay_on = false;

void set_relay(bool on) {
    relay_on = on;
    digitalWrite(RELAY_PIN, on ? HIGH : LOW);
    Serial.printf("[relay] %s\n", on ? "ON" : "OFF");
}

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
        PMU.isVbusIn(),
        relay_on,
    };
}

// JSON has no NaN, so failed reads are sent as null.
void format_json_float(char* buf, size_t len, float value) {
    if (isnan(value)) snprintf(buf, len, "null");
    else snprintf(buf, len, "%.1f", value);
}

void on_config(char* topic, byte* payload, unsigned int len) {
    Serial.printf("[config] %.*s\n", (int)len, (char*)payload);
    // Test hook until the heating logic exists: {"relay":true} / {"relay":false}
    String msg((const char*)payload, len);
    msg.replace(" ", "");
    if (msg.indexOf("\"relay\":true") >= 0) set_relay(true);
    else if (msg.indexOf("\"relay\":false") >= 0) set_relay(false);
    int i = msg.indexOf("\"sleep_s\":");
    if (i >= 0) set_sleep_s(msg.substring(i + 10).toInt());
}

// Bench test commands over USB serial: "relay on" / "relay off".
void handle_serial_command(String line) {
    line.trim();
    if (line == "relay on") set_relay(true);
    else if (line == "relay off") set_relay(false);
    else if (line.length()) Serial.printf("[serial] unknown command: %s\n", line.c_str());
}

// Like delay(), but keeps reading serial commands.
void usb_wait(uint32_t ms) {
    static String line;
    uint32_t start = millis();
    while (millis() - start < ms) {
        while (Serial.available()) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
                handle_serial_command(line);
                line = "";
            } else if (line.length() < 32) {
                line += c;
            }
        }
        if (mqtt.connected()) mqtt.loop();  // config messages arrive here
        delay(50);
    }
}

// Powers the modem, joins the network, syncs the clock and connects MQTT with the config subscription.
bool mqtt_open() {
    Serial.println("--- Connect start ---");

    Serial.println("[1/3] Powering modem...");
    PMU_setup(true);
    basic_modem_setup(modem);
    modem.sendAT(GF("+CSCLK=0")); modem.waitResponse();

    Serial.println("[2/3] Connecting to network...");
    if (!connect_GSM_1nce(modem)) return false;

    Serial.println("[3/3] Connecting MQTT...");
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
    mqtt.setBufferSize(1280);
    mqtt.setKeepAlive(120);
    mqtt.setCallback(on_config);

    if (!mqtt.connect(MQTT_CLIENT_ID)) {
        Serial.printf("MQTT connect failed, rc=%d\n", mqtt.state());
        return false;
    }

    mqtt.subscribe(MQTT_CONFIG_TOPIC);
    sslClient.flush();  // send SUBSCRIBE to broker immediately
    delay(1000);        // wait for SUBACK + retained PUBLISH over LTE
    mqtt.loop();        // process SUBACK (one SSL record)
    mqtt.loop();        // process retained PUBLISH → on_config fires
    Serial.println("--- Connected ---");
    return true;
}

bool publish_readings() {
    Serial.printf("Publishing %d readings...\n", measurement_count);
    // Build JSON: {"readings":[{"ts":0,"temp":23.1,"hum":48.0,"batt_mv":3950,"batt_pct":82,"charging":false,"vbus":true,"relay":false},...]}
    char payload[1024];
    int pos = snprintf(payload, sizeof(payload), "{\"readings\":[");
    for (int i = 0; i < measurement_count; i++) {
        const SensorReading& r = measurements[i];
        char temp[8], hum[8];
        format_json_float(temp, sizeof(temp), r.temp_c);
        format_json_float(hum, sizeof(hum), r.humidity);
        pos += snprintf(payload + pos, sizeof(payload) - pos,
                        "%s{\"ts\":%lu,\"temp\":%s,\"hum\":%s,\"batt_mv\":%u,\"batt_pct\":%d,\"charging\":%s,\"vbus\":%s,\"relay\":%s}",
                        i ? "," : "", r.ts, temp, hum,
                        r.batt_mv, r.batt_pct, r.charging ? "true" : "false",
                        r.vbus ? "true" : "false", r.relay ? "true" : "false");
    }
    snprintf(payload + pos, sizeof(payload) - pos, "]}");

    bool ok = mqtt.publish(MQTT_TOPIC, payload);
    mqtt.loop();  // flush BearSSL buffer → AT+CASEND
    Serial.printf("Publish %s\n", ok ? "OK" : "FAILED");
    return ok;
}

void mqtt_close() {
    mqtt.disconnect();
    modem.gprsDisconnect();
    PMU.disableDC3();  // power down modem — PMU holds DC3 on independently
    Serial.println("--- Disconnected ---");
}

void setup() {
    // Relay first: after deep sleep the pin is still an RTC pin with the sleep pull-down.
    rtc_gpio_deinit((gpio_num_t)RELAY_PIN);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    Serial.begin(115200);
    // Init PMU once per boot or wake — needed for battery readings; modem enabled later if sending.
    // Not in loop(): it switches the modem off, which would drop the connection we keep on USB.
    PMU_setup(false);
    load_settings();
    dht.begin();
}

void loop() {

    SensorReading r = read_sensors();
    measurements[measurement_count++] = r;
    Serial.printf("[reading %d/%d] ts=%lu temp=%.1f hum=%.1f batt=%dmV %d%%%s vbus=%d relay=%d\n",
                  measurement_count, SEND_EVERY_N, r.ts, r.temp_c, r.humidity,
                  r.batt_mv, r.batt_pct, r.charging ? " charging" : "", r.vbus, r.relay);

    // USB takes a moment to come back after a wake, so give the host time to appear before deciding to sleep.
    // isPlugged() checks for USB frames from the host. Serial's bool can briefly read false while connected.
    uint32_t usb_wait_start = millis();
    delay(200);
    while (!Serial.isPlugged() && millis() - usb_wait_start < 2000) delay(50);
    bool usb = Serial.isPlugged();

    // On USB: send every reading and stay connected, so config messages (relay) arrive within a second.
    if (usb || measurement_count >= SEND_EVERY_N) {
        bool ok = mqtt.connected() || mqtt_open();
        if (ok) ok = publish_readings();
        if (ok || measurement_count >= SEND_EVERY_N) measurement_count = 0;  // drop a full batch rather than get stuck
        if (!usb && mqtt.connected()) mqtt_close();
        else if (!usb) PMU.disableDC3();
    }

    // When USB is connected: wait instead of deep sleep so the port stays visible for upload.
    if (usb) {
        int usb_delay = 300 * 1000;
        Serial.printf("[sleep] USB detected — waiting %ds instead of sleeping.\n", usb_delay / 1000);
        next_ts += usb_delay / 1000;
        usb_wait(usb_delay);
        return;
    }

    next_ts += sleep_s;
    Serial.printf("[sleep] %lus. Next send in %d readings.\n",
                  sleep_s, SEND_EVERY_N - measurement_count);
    Serial.flush();
    // Never sleep with the relay on, and hold it low with the RTC pull-down as well as the external one.
    set_relay(false);
    rtc_gpio_init((gpio_num_t)RELAY_PIN);
    rtc_gpio_set_direction((gpio_num_t)RELAY_PIN, RTC_GPIO_MODE_DISABLED);
    rtc_gpio_pullup_dis((gpio_num_t)RELAY_PIN);
    rtc_gpio_pulldown_en((gpio_num_t)RELAY_PIN);
    // BOOT button = GPIO0, active LOW. Enable RTC pull-up so the pin isn't floating during sleep.
    // The pull-up only holds if the RTC peripherals stay powered; otherwise the pin floats low and wakes us at once.
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    rtc_gpio_pullup_en(GPIO_NUM_0);
    rtc_gpio_pulldown_dis(GPIO_NUM_0);
    esp_sleep_enable_ext1_wakeup(1ULL << 0, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_sleep_enable_timer_wakeup((uint64_t)sleep_s * 1000000ULL);
    esp_deep_sleep_start();
}
