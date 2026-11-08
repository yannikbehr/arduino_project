# ESP32-S3 IoT Sensor

## What it does

The device wakes every 10 minutes, reads temperature and humidity from a DHT11 (data on GPIO 44), and goes back to sleep. Once per hour (every 6 readings) it connects over LTE, sends a batch of measurements to AWS IoT Core via MQTT over mTLS, then powers down the modem and sleeps again.

On USB it doesn't sleep: it sends every reading and stays connected, so config messages arrive within a second.

A relay board input on GPIO 21 (100k pull-down to GND) switches the floor heating. The relay is off at boot and before every deep sleep.

While the relay is on, the device stays awake and connected, so `heat-off` takes effect within seconds; it sends a reading every `sleep_s`. Safety:

- The connection is checked every minute (MQTT keepalive 30 s). After 3 failed reconnects the relay switches off and the device goes back to the deep-sleep cycle. It switches on again only after a successful connect delivers the retained `relay:true`.
- A task watchdog resets the board after 5 minutes without progress; the relay is off at boot.
- Every relay change is reported at once as a reading with an `event` field (`command`, `serial`, `cutoff`). A cutoff while offline is queued and sent with the next successful connect.

### Config topic

The device subscribes to `devices/SIM7080G_01/config` on every connect. Publish retained messages so the device picks them up after a power cut:

```
aws iot-data publish --region eu-west-3 --endpoint-url https://a3fu7j5avgf87g-ats.iot.eu-west-3.amazonaws.com \
  --topic devices/SIM7080G_01/config --qos 1 --retain --payload '{"sleep_s":600,"relay":false}'
```

- `relay`: `true` / `false` switches the relay.
- `sleep_s`: seconds between readings, accepted from 30 to 7200 (anything else is ignored). Saved in flash, so it survives a power cut.

Shortcuts, which read the current config and change one key: `make heat-on`, `make heat-off`, `make sleep SLEEP_S=900`, `make config` (show).

On USB serial, `relay on` / `relay off` switch the relay for bench tests.

## Infrastructure

```
Device (LilyGO T-SIM7080G)
  └─ LTE (1NCE SIM) ──► AWS IoT Core (MQTT, port 8883)
                              └─ IoT Rule ──► Lambda (esp32_sensor_processor)
                                                  └─ DynamoDB (esp32_sensor_readings)
```

- **Connectivity**: 1NCE LTE SIM, MQTT over mTLS with device certificate issued by AWS IoT
- **TLS**: Handled on the ESP32 by BearSSL (SSLClient library) — the modem is used as a plain TCP pipe
- **AWS certs**: Managed in Terraform, downloaded with `make certs`, converted to C headers with `make certs-der`

## Uploading firmware

The USB port disappears when the device is in deep sleep. To upload:

1. Plug in the USB cable
2. Press the **BOOT** button — this wakes the device immediately
3. The device detects USB is connected and stays awake
4. Run `make upload` any time

To resume normal operation, unplug the USB cable and press **RESET**.
