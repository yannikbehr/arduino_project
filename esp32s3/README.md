# ESP32-S3 IoT Sensor

## What it does

The device wakes every 10 minutes, reads two temperature sensors, and goes back to sleep. Once per hour (every 6 readings) it connects over LTE, sends a batch of measurements to AWS IoT Core via MQTT over mTLS, then powers down the modem and sleeps again.

## Infrastructure

```
Device (LilyGO T-SIM7080G)
  └─ LTE (1NCE SIM) ──► AWS IoT Core (MQTT, port 8883)
                              └─ IoT Rule ──► Lambda (esp32_sensor_processor)
                                                  └─ DynamoDB
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
