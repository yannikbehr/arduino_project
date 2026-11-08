#include <Arduino.h>
#include "driver/rtc_io.h"
#include "modem.h"
#include <SSLClient.h>
#include "ssl_certs_der.h"
#include <PubSubClient.h>
#include <DHT.h>
#include <Preferences.h>
#include <esp_task_wdt.h>

#define MQTT_BROKER    "a3fu7j5avgf87g-ats.iot.eu-west-3.amazonaws.com"
#define MQTT_PORT      8883
#define MQTT_CLIENT_ID "T-SIM7080G_01"
#define MQTT_TOPIC     "devices/SIM7080G_01/data"
#define MQTT_CONFIG_TOPIC "devices/SIM7080G_01/config"

// --- Timing (swap these for production) ---
#define SLEEP_INTERVAL_S   600         // default; overridden by "sleep_s" in the config topic
#define SLEEP_MIN_S        30          // accepted range for "sleep_s": a too-long sleep would lose remote control
#define SLEEP_MAX_S        7200

// --- Heating safety ---
#define CONNECT_ATTEMPTS   3           // while heating: this many failed connects in a row switch the relay off
#define LINK_CHECK_MS      60000       // while awake: check the connection at least this often
#define MQTT_KEEPALIVE_S   30          // a dead link is noticed within about 1.5x this
#define WDT_TIMEOUT_S      300         // a hang resets the board, and the relay is off at boot
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
    char     event[8];  // why the relay changed just before this reading: "command", "serial", "cutoff", or ""
};

// --- Persistent state across deep sleeps ---
RTC_DATA_ATTR static SensorReading measurements[SEND_EVERY_N];
RTC_DATA_ATTR static int            measurement_count = 0;
RTC_DATA_ATTR static uint32_t       next_ts = 0;  // estimated Unix time of next reading, synced on each send
static uint32_t next_ts_ms = 0;  // millis() when next_ts was last advanced, for keeping time while awake

// Moves next_ts forward by the time spent awake since it was last set.
void advance_clock() {
    uint32_t elapsed_s = (millis() - next_ts_ms) / 1000;
    next_ts += elapsed_s;
    next_ts_ms += elapsed_s * 1000;
}

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
// Set when the relay changes, so the next reading is taken and sent at once.
static const char* relay_event = nullptr;

void set_relay(bool on, const char* why) {
    digitalWrite(RELAY_PIN, on ? HIGH : LOW);
    if (on == relay_on) return;
    relay_on = on;
    relay_event = why;
    Serial.printf("[relay] %s (%s)\n", on ? "ON" : "OFF", why);
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
    SensorReading r = {};
    r.ts = next_ts;
    r.temp_c = temp_c;
    r.humidity = humidity;
    r.batt_mv = PMU.getBattVoltage();
    r.batt_pct = PMU.getBatteryPercent();
    r.charging = PMU.isCharging();
    r.vbus = PMU.isVbusIn();
    r.relay = relay_on;
    return r;
}

// JSON has no NaN, so failed reads are sent as null.
void format_json_float(char* buf, size_t len, float value) {
    if (isnan(value)) snprintf(buf, len, "null");
    else snprintf(buf, len, "%.1f", value);
}

void on_config(char* topic, byte* payload, unsigned int len) {
    Serial.printf("[config] %.*s\n", (int)len, (char*)payload);
    // {"relay":true} keeps the device awake and connected with the relay on; {"relay":false} lets it sleep again.
    String msg((const char*)payload, len);
    msg.replace(" ", "");
    if (msg.indexOf("\"relay\":true") >= 0) set_relay(true, "command");
    else if (msg.indexOf("\"relay\":false") >= 0) set_relay(false, "command");
    int i = msg.indexOf("\"sleep_s\":");
    if (i >= 0) set_sleep_s(msg.substring(i + 10).toInt());
}

// Bench test commands over USB serial: "relay on" / "relay off".
void handle_serial_command(String line) {
    line.trim();
    if (line == "relay on") set_relay(true, "serial");
    else if (line == "relay off") set_relay(false, "serial");
    else if (line.length()) Serial.printf("[serial] unknown command: %s\n", line.c_str());
}

// Stays awake for ms, handling serial commands and config messages and checking the connection every minute.
// Returns early once the relay is off and we're not on USB, so the device can go back to deep sleep.
void awake_wait(uint32_t ms, bool usb) {
    static String line;
    uint32_t start = millis();
    uint32_t last_check = millis();
    while (millis() - start < ms) {
        esp_task_wdt_reset();
        while (Serial.available()) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
                handle_serial_command(line);
                line = "";
            } else if (line.length() < 32) {
                line += c;
            }
        }
        if (mqtt.connected()) mqtt.loop();  // config messages arrive here; also sends keepalive pings
        if (relay_event) return;            // report the change at once

        if (millis() - last_check >= LINK_CHECK_MS) {
            last_check = millis();
            if (!mqtt.connected()) {
                Serial.printf("[link] connection lost (state %d), reconnecting\n", mqtt.state());
                if (!mqtt_connect(relay_on ? CONNECT_ATTEMPTS : 1)) {
                    if (relay_on) {
                        Serial.printf("[link] %d connects failed, switching heating off\n", CONNECT_ATTEMPTS);
                        set_relay(false, "cutoff");
                    }
                    if (!usb) return;
                }
            }
        }
        delay(50);
    }
}

// Powers the modem, joins the network, syncs the clock and connects MQTT with the config subscription.
bool mqtt_open() {
    Serial.println("--- Connect start ---");
    esp_task_wdt_reset();

    Serial.println("[1/3] Powering modem...");
    PMU_setup(true);
    basic_modem_setup(modem);
    modem.sendAT(GF("+CSCLK=0")); modem.waitResponse();

    Serial.println("[2/3] Connecting to network...");
    if (!connect_GSM_1nce(modem, 3)) return false;

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
        next_ts_ms = millis();
    } else {
        Serial.println("Network time unavailable, using build time.");
    }
    sslClient.setMutualAuthParams(mTLS);
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setBufferSize(1280);
    mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
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

// After a failed send with a full batch: keep only readings that record a relay change.
void drop_plain_readings() {
    int kept = 0;
    for (int i = 0; i < measurement_count; i++) {
        if (measurements[i].event[0]) measurements[kept++] = measurements[i];
    }
    Serial.printf("Send failed, dropped %d readings, kept %d relay events\n", measurement_count - kept, kept);
    measurement_count = kept;
}

// Connects unless already connected, trying up to attempts times.
bool mqtt_connect(int attempts) {
    if (mqtt.connected()) return true;
    for (int i = 1; i <= attempts; i++) {
        if (mqtt_open()) return true;
        Serial.printf("[connect] attempt %d/%d failed\n", i, attempts);
    }
    return false;
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
                        "%s{\"ts\":%lu,\"temp\":%s,\"hum\":%s,\"batt_mv\":%u,\"batt_pct\":%d,\"charging\":%s,\"vbus\":%s,\"relay\":%s%s%s%s}",
                        i ? "," : "", r.ts, temp, hum,
                        r.batt_mv, r.batt_pct, r.charging ? "true" : "false",
                        r.vbus ? "true" : "false", r.relay ? "true" : "false",
                        r.event[0] ? ",\"event\":\"" : "", r.event, r.event[0] ? "\"" : "");
    }
    snprintf(payload + pos, sizeof(payload) - pos, "]}");
    if (pos >= (int)sizeof(payload) - 2) Serial.println("Payload truncated!");

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

// Takes a reading, tags it with a pending relay change, and queues it (kept across deep sleep).
// Returns true if it records a relay change, which should be sent at once.
bool record_reading() {
    SensorReading r = read_sensors();
    bool changed = relay_event != nullptr;
    if (changed) {
        strlcpy(r.event, relay_event, sizeof(r.event));
        relay_event = nullptr;
    }
    if (measurement_count >= SEND_EVERY_N) {  // queue full after failed sends: keep the newest
        memmove(measurements, measurements + 1, sizeof(SensorReading) * (SEND_EVERY_N - 1));
        measurement_count--;
    }
    measurements[measurement_count++] = r;
    Serial.printf("[reading %d/%d] ts=%lu temp=%.1f hum=%.1f batt=%dmV %d%%%s vbus=%d relay=%d\n",
                  measurement_count, SEND_EVERY_N, r.ts, r.temp_c, r.humidity,
                  r.batt_mv, r.batt_pct, r.charging ? " charging" : "", r.vbus, r.relay);

    return changed;
}

void setup() {
    // Relay first: after deep sleep the pin is still an RTC pin with the sleep pull-down.
    rtc_gpio_deinit((gpio_num_t)RELAY_PIN);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    Serial.begin(115200);
    esp_task_wdt_init(WDT_TIMEOUT_S, true);  // panic → reset
    esp_task_wdt_add(NULL);                  // watch the loop task
    // Init PMU once per boot or wake — needed for battery readings; modem enabled later if sending.
    // Not in loop(): it switches the modem off, which would drop the connection we keep on USB.
    PMU_setup(false);
    load_settings();
    dht.begin();
}

void loop() {
    esp_task_wdt_reset();
    advance_clock();

    bool changed = record_reading();

    // USB takes a moment to come back after a wake, so give the host time to appear before deciding to sleep.
    // isPlugged() checks for USB frames from the host. Serial's bool can briefly read false while connected.
    uint32_t usb_wait_start = millis();
    delay(200);
    while (!Serial.isPlugged() && millis() - usb_wait_start < 2000) delay(50);
    bool usb = Serial.isPlugged();

    // Stay awake and connected on USB or while heating, sending every reading so commands arrive at once.
    // Otherwise send a batch every SEND_EVERY_N readings and deep sleep in between.
    bool awake = usb || relay_on;
    if (awake || changed || measurement_count >= SEND_EVERY_N) {
        bool ok = mqtt_connect(relay_on ? CONNECT_ATTEMPTS : 1);
        if (!ok && relay_on) {
            Serial.printf("[connect] %d connects failed, switching heating off\n", CONNECT_ATTEMPTS);
            set_relay(false, "cutoff");
        }
        if (ok) ok = publish_readings();
        if (ok) measurement_count = 0;
        else if (measurement_count >= SEND_EVERY_N) drop_plain_readings();  // don't retry every wake, but keep relay events
        awake = usb || relay_on;
        // The retained config may have switched the relay while connecting: report that straight away.
        if (relay_event && mqtt.connected()) return;
    }

    if (awake) {
        uint32_t wait_s = usb ? 300 : sleep_s;
        Serial.printf("[awake] %s — next reading in %lus.\n", usb ? "USB" : "heating on", wait_s);
        awake_wait(wait_s * 1000, usb);
        return;
    }

    if (mqtt.connected()) mqtt_close();
    else PMU.disableDC3();  // modem may still be powered after a failed connect

    // e.g. a cutoff: the flag doesn't survive deep sleep, the queued reading does
    if (relay_event) {
        advance_clock();
        record_reading();
    }
    next_ts += sleep_s;
    Serial.printf("[sleep] %lus. Next send in %d readings.\n",
                  sleep_s, SEND_EVERY_N - measurement_count);
    Serial.flush();
    // Never sleep with the relay on, and hold it low with the RTC pull-down as well as the external one.
    set_relay(false, "sleep");  // already off by now; just makes sure
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
