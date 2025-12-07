#include <math.h>
#include <stdio.h>

#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Modules
#include "gui.h"
#include "mqtt_helper.h"
#include "sensor.h"
#include "wifi_helper.h"

static const char *TAG = "MAIN";

// Settings
#define SEND_INTERVAL_HEARTBEAT_US CONFIG_SEND_INTERVAL_HEARTBEAT_US
// Thresholds are stored as integers (1 = 0.1, 5 = 0.5, etc.)
#define THRESHOLD_TEMP (CONFIG_THRESHOLD_TEMP * 0.1f)
#define THRESHOLD_HUM (CONFIG_THRESHOLD_HUM * 0.1f)
#define BUTTON_GPIO CONFIG_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL CONFIG_BUTTON_ACTIVE_LEVEL

int64_t last_send_time = 0; // Global so it can be reset if needed

void app_main(void)
{
    // --- 1. Hardware init ---
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gui_init();
    gui_set_status("Booting...");

    // --- 2. Button check (reset WiFi credentials) ---
    if (gpio_get_level(BUTTON_GPIO) == BUTTON_ACTIVE_LEVEL)
    {
        ESP_LOGI(TAG, "Button pressed during boot! Erasing WiFi credentials...");
        gui_set_status("Erasing WiFi...");
        vTaskDelay(pdMS_TO_TICKS(2000));  // Show message and debounce
        wifi_helper_reset_provisioning(); // This will restart the device
    }

    // --- 3. Init modules ---
    wifi_helper_init();
    sensor_init();
    // mqtt_helper_start() is called later once Wi-Fi is up

    float current_temp = 0.0;
    float current_hum = 0.0;
    float last_sent_temp = -127.0;
    float last_sent_hum = -1.0;

    // Status flags
    bool mqtt_started = false;

    while (1)
    {
        // ---------------------------------------------------------
        // A. Wi-Fi check
        // ---------------------------------------------------------
        if (wifi_helper_is_connected())
        {
            // Start MQTT once Wi-Fi is up
            if (!mqtt_started)
            {
                ESP_LOGI(TAG, "WiFi ready, starting MQTT...");
                mqtt_helper_start();
                mqtt_started = true;
            }

            // ---------------------------------------------------------
            // B. Read sensor
            // ---------------------------------------------------------
            if (sensor_read_values(&current_temp, &current_hum))
            {
                // Update display immediately, even without MQTT
                gui_set_values(current_temp, current_hum);

                // ---------------------------------------------------------
                // C. MQTT check & send
                // ---------------------------------------------------------
                // Ensure MQTT is connected before publishing
                if (mqtt_helper_is_connected())
                {
                    // Delta Check
                    bool diff_temp = fabs(current_temp - last_sent_temp) >= THRESHOLD_TEMP;
                    bool diff_hum = fabs(current_hum - last_sent_hum) >= THRESHOLD_HUM;

                    // Heartbeat check
                    int64_t now = esp_timer_get_time();
                    bool heartbeat = (now - last_send_time) > SEND_INTERVAL_HEARTBEAT_US;

                    if (diff_temp || diff_hum || heartbeat)
                    {
                        gui_set_status("Sending MQTT...");

                        // snprintf with %.1f formats values for MQTT payload
                        mqtt_helper_send_data(current_temp, current_hum);

                        last_sent_temp = current_temp;
                        last_sent_hum = current_hum;
                        last_send_time = now;

                        ESP_LOGI(TAG, "Update sent. T:%.1f H:%.1f (Reason: %s)",
                                 current_temp, current_hum,
                                 heartbeat ? "Timer" : "Change");
                    }
                    else
                    {
                        gui_set_status("Online (Idle)");
                    }
                }
                else
                {
                    // Wi-Fi is ready, MQTT still connecting
                    gui_set_status("Connecting MQTT...");
                }
            }
            else
            {
                gui_set_status("Sensor Error");
            }
        }
        else
        {
            // No Wi-Fi
            gui_set_status("Waiting for WiFi...");
            // If Wi-Fi drops, mqtt_started could be reset, but ESP-IDF handles reconnects well
        }

        // Runtime button check (disabled)
        // if (gpio_get_level(BUTTON_GPIO) == BUTTON_ACTIVE_LEVEL)
        // {
        //     vTaskDelay(pdMS_TO_TICKS(2000));
        //     if (gpio_get_level(BUTTON_GPIO) == BUTTON_ACTIVE_LEVEL)
        //     {
        //         gui_set_status("Resetting...");
        //         wifi_helper_reset_provisioning();
        //     }
        // }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
