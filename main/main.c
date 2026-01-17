#include <math.h>
#include <stdio.h>

#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "wifi_provisioning/manager.h"

// Modules
#include "gui.h"
#include "mqtt_helper.h"
#include "sensor.h"
#include "wifi_helper.h"

static const char *TAG = "MAIN";

// Settings
#define SEND_INTERVAL_HEARTBEAT_US CONFIG_SEND_INTERVAL_HEARTBEAT_US
#define THRESHOLD_TEMP (CONFIG_THRESHOLD_TEMP * 0.1f)
#define THRESHOLD_HUM (CONFIG_THRESHOLD_HUM * 0.1f)
#define BUTTON_GPIO CONFIG_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL CONFIG_BUTTON_ACTIVE_LEVEL
#define LONG_PRESS_DURATION_MS 3000

int64_t last_send_time = 0;
volatile bool button_pressed = false;
volatile bool provisioning_reset_triggered = false;

// Button interrupt handler
static void IRAM_ATTR button_isr_handler(void *arg)
{
    button_pressed = true;
}

// Dedicated task for button handling
static void button_task(void *arg)
{
    while (1)
    {
        if (button_pressed)
        {
            button_pressed = false;

            if (gpio_get_level(BUTTON_GPIO) == BUTTON_ACTIVE_LEVEL)
            {
                int64_t press_start = esp_timer_get_time();
                bool is_long_press = false;

                while (gpio_get_level(BUTTON_GPIO) == BUTTON_ACTIVE_LEVEL)
                {
                    vTaskDelay(pdMS_TO_TICKS(100));
                    int64_t press_duration = (esp_timer_get_time() - press_start) / 1000;

                    if (press_duration >= LONG_PRESS_DURATION_MS)
                    {
                        // Long press detected
                        ESP_LOGI(TAG, "Long press detected - resetting WiFi provisioning");

                        // Set status and block main loop
                        gui_set_status("Resetting WiFi...");
                        provisioning_reset_triggered = true;

                        vTaskDelay(pdMS_TO_TICKS(500));

                        // This will erase WiFi credentials and restart the device
                        wifi_helper_reset_provisioning();

                        is_long_press = true;
                        break;
                    }
                }

                // Short press - toggle display
                if (!is_long_press)
                {
                    ESP_LOGI(TAG, "Short press - toggling display");
                    if (gui_is_enabled())
                        gui_turn_off();
                    else
                        gui_turn_on();
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    // --- 1. Hardware init ---
    gpio_config_t io_conf = {};
    io_conf.intr_type = (BUTTON_ACTIVE_LEVEL == 0) ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);

    gui_init();
    gui_set_status("Booting...");

    // --- 3. Init modules ---
    wifi_helper_init();
    sensor_init();

    xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Button task started");

    float current_temp = 0.0;
    float current_hum = 0.0;
    float last_sent_temp = -127.0;
    float last_sent_hum = -1.0;

    bool mqtt_started = false;

    while (1)
    {
        // Read and display sensor values
        if (sensor_read_values(&current_temp, &current_hum))
        {
            gui_set_values(current_temp, current_hum);
        }
        else
        {
            if (!provisioning_reset_triggered && wifi_helper_is_connected())
            {
                gui_set_status("Sensor Error");
            }
        }

        if (!provisioning_reset_triggered)
        {
            if (wifi_helper_is_connected())
            {
                // Start MQTT
                if (!mqtt_started)
                {
                    ESP_LOGI(TAG, "WiFi ready, starting MQTT...");
                    mqtt_helper_start();
                    mqtt_started = true;
                }

                // MQWTT connected -> send data if needed
                if (mqtt_helper_is_connected())
                {
                    bool diff_temp = fabs(current_temp - last_sent_temp) >= THRESHOLD_TEMP;
                    bool diff_hum = fabs(current_hum - last_sent_hum) >= THRESHOLD_HUM;
                    int64_t now = esp_timer_get_time();
                    bool heartbeat = (now - last_send_time) > SEND_INTERVAL_HEARTBEAT_US;

                    if (diff_temp || diff_hum || heartbeat)
                    {
                        gui_set_status("Sending MQTT...");
                        mqtt_helper_send_data(current_temp, current_hum);

                        last_sent_temp = current_temp;
                        last_sent_hum = current_hum;
                        last_send_time = now;

                        ESP_LOGI(TAG, "Update sent. T:%.1f H:%.1f", current_temp, current_hum);
                    }
                    else
                    {
                        gui_set_status("Online (Idle)");
                    }
                }
                else
                {
                    gui_set_status("Connecting MQTT...");
                }
            }
            else // Not connected to WiFi
            {
                bool provisioned = false;
                // Check if there are WiFi credentials stored
                esp_err_t err = wifi_prov_mgr_is_provisioned(&provisioned);

                if (err == ESP_OK && !provisioned)
                {
                    gui_set_status("Provisioning Mode");
                }
                else
                {
                    gui_set_status("Waiting for WiFi...");
                }
            }
        }

        // Delay before next iteration
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
