#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

// Modules
#include "gui.h"
#include "sensor.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    // 1. Start GUI
    gui_init();
    gui_set_status("Init Sensor...");

    // 2. Initialize Sensor
    sensor_init();

    // Wait for the sensor to stabilize (DHT needs about 1-2 seconds after power-on)
    vTaskDelay(pdMS_TO_TICKS(2000));

    gui_set_status("Sensor Active");

    float temp = 0.0;
    float hum = 0.0;

    while (1)
    {
        // 3. Read Sensor
        if (sensor_read_values(&temp, &hum))
        {
            // Success: Display values
            gui_set_values(temp, hum);
            ESP_LOGI(TAG, "Temp: %.1f, Hum: %.1f", temp, hum);
        }
        else
        {
            // Error: Show user that something is wrong
            ESP_LOGW(TAG, "Sensor Error!");
            gui_set_status("Sensor Error!");
        }

        // 4. Wait before next read (DHT max 0.5Hz)
        vTaskDelay(pdMS_TO_TICKS(2500));
    }
}
