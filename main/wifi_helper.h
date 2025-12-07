#pragma once
#include <stdbool.h>

// Initializes NVS and Wi-Fi.
// Returns true when Wi-Fi is connected.
// Returns false when provisioning is started.
void wifi_helper_init(void);

// Erases stored Wi-Fi credentials and restarts
void wifi_helper_reset_provisioning(void);

// Returns true when connected (used by main loop)
bool wifi_helper_is_connected(void);
