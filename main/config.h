#pragma once

/**
 * Configuration header for IoT Sensor project
 *
 * This file includes settings from Kconfig (ESP-IDF menuconfig)
 * and provides sensible defaults for the sensor application.
 */

#include "sdkconfig.h"

// ============ DEVICE CONFIGURATION ============

#if CONFIG_DEVICE_NAME_CUSTOM
#define DEVICE_FRIENDLY_NAME CONFIG_DEVICE_NAME
#else
// Auto-generated name from MAC address suffix (set at runtime)
#define DEVICE_FRIENDLY_NAME NULL
#endif

// ============ MQTT CONFIGURATION ============

#define MQTT_BROKER_URI CONFIG_MQTT_BROKER_URI
#define MQTT_USER CONFIG_MQTT_USER
#define MQTT_PASS CONFIG_MQTT_PASS

// ============ WI-FI PROVISIONING CONFIGURATION ============

#define WIFI_PROV_SERVICE_NAME CONFIG_WIFI_PROV_SERVICE_NAME

// ============ SENSOR & MQTT SETTINGS ============

#define SEND_INTERVAL_HEARTBEAT_US CONFIG_SEND_INTERVAL_HEARTBEAT_US
// Thresholds are stored as integers (1 = 0.1, 5 = 0.5, etc.)
#define THRESHOLD_TEMP (CONFIG_THRESHOLD_TEMP * 0.1f)
#define THRESHOLD_HUM (CONFIG_THRESHOLD_HUM * 0.1f)

// ============ HARDWARE PIN CONFIGURATION ============

#define BUTTON_GPIO CONFIG_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL CONFIG_BUTTON_ACTIVE_LEVEL

// ============ GUI CONFIGURATION ============

// I2C settings for display
#define I2C_BUS_PORT CONFIG_I2C_BUS_PORT
#define PIN_NUM_SDA CONFIG_PIN_NUM_SDA
#define PIN_NUM_SCL CONFIG_PIN_NUM_SCL
#define PIN_NUM_RST CONFIG_PIN_NUM_RST
#define I2C_HW_ADDR CONFIG_I2C_HW_ADDR

// Display settings
#define LCD_H_RES CONFIG_LCD_H_RES
#define LCD_V_RES CONFIG_LCD_V_RES
#define LCD_PIXEL_CLOCK_HZ CONFIG_LCD_PIXEL_CLOCK_HZ

// ============ SENSOR CONFIGURATION ============

// DHT sensor GPIO pin
#define SENSOR_GPIO CONFIG_SENSOR_GPIO
#define SENSOR_TYPE DHT_TYPE_AM2301
