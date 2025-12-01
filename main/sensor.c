#include "sensor.h"
#include "dht.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define SENSOR_TYPE DHT_TYPE_AM2301 // AM2301 is compatible with DHT22
#define SENSOR_GPIO 10

static const char *TAG = "SENSOR";

void sensor_init(void)
{
    gpio_set_pull_mode(SENSOR_GPIO, GPIO_PULLUP_ONLY);
    ESP_LOGI(TAG, "Sensor init on GPIO %d", SENSOR_GPIO);
}

bool sensor_read_values(float *temperature, float *humidity)
{
    esp_err_t res = dht_read_float_data(SENSOR_TYPE, SENSOR_GPIO, humidity, temperature);

    if (res == ESP_OK)
    {
        ESP_LOGD(TAG, "Read: %.1f degC, %.1f %%", *temperature, *humidity);
        return true;
    }
    else
    {
        ESP_LOGE(TAG, "Could not read data from sensor: %d", res);
        return false;
    }
}
