#pragma once
#include <stdbool.h>

// Starts the MQTT client
void mqtt_helper_start(void);

// Sends Home Assistant auto-discovery config
void mqtt_helper_send_discovery(void);

// Sends current measurement values
void mqtt_helper_send_data(float temp, float hum);

// Returns true when connected to the broker
bool mqtt_helper_is_connected(void);
