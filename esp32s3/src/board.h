#pragma once

// LilyGO T-SIM7080G pin definitions
#define I2C_SDA             15
#define I2C_SCL              7

#define BOARD_MODEM_PWR_PIN 41
#define BOARD_MODEM_RXD_PIN  4
#define BOARD_MODEM_TXD_PIN  5

#define DHT_PIN             44

// Relay board input (NPN base via the board's own resistor). External 100k pull-down keeps it off
// while the pin floats during reset, flashing and deep sleep.
#define RELAY_PIN           21
