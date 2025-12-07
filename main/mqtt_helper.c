#include "mqtt_helper.h"

#include <stdio.h>

#include "cJSON.h"
#include "config.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT";
static esp_mqtt_client_handle_t client = NULL;
static bool s_mqtt_connected = false;

// Dynamic identifiers so multiple devices can coexist in Home Assistant
static bool ids_ready = false;
static char device_id[32];
static char device_name[48];
static char topic_state[96];
static char topic_conf_temp[96];
static char topic_conf_hum[96];
static char uniq_id_temp[48];
static char uniq_id_hum[48];
static char topic_lwt[96];

static void init_identifiers(void)
{
    if (ids_ready)
        return;

    uint8_t mac[6] = {0};
    esp_efuse_mac_get_default(mac);

    // Use last 3 bytes of the MAC as a suffix to keep topics and IDs unique per device
    snprintf(device_id, sizeof(device_id), "esp32-sensor-%02X%02X%02X", mac[3], mac[4], mac[5]);

    // Use custom name if configured, otherwise auto-generate from MAC
    if (DEVICE_FRIENDLY_NAME != NULL)
    {
        snprintf(device_name, sizeof(device_name), "%s", (const char *)DEVICE_FRIENDLY_NAME);
    }
    else
    {
        snprintf(device_name, sizeof(device_name), "ESP32 Sensor %02X%02X%02X", mac[3], mac[4], mac[5]);
    }

    snprintf(topic_state, sizeof(topic_state), "homeassistant/sensor/%s/state", device_id);
    snprintf(topic_conf_temp, sizeof(topic_conf_temp), "homeassistant/sensor/%s_temp/config", device_id);
    snprintf(topic_conf_hum, sizeof(topic_conf_hum), "homeassistant/sensor/%s_hum/config", device_id);
    snprintf(topic_lwt, sizeof(topic_lwt), "homeassistant/sensor/%s/availability", device_id);

    snprintf(uniq_id_temp, sizeof(uniq_id_temp), "%s-temp", device_id);
    snprintf(uniq_id_hum, sizeof(uniq_id_hum), "%s-hum", device_id);

    ids_ready = true;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args; // Unused
    (void)base;         // Unused
    (void)event_data;   // Unused

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Connected");
        s_mqtt_connected = true;
        // Publish online status and send discovery payloads
        esp_mqtt_client_publish(client, topic_lwt, "online", 6, 1, 1);
        mqtt_helper_send_discovery();
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT Disconnected");
        s_mqtt_connected = false;
        break;
    default:
        break;
    }
}

void mqtt_helper_start(void)
{
    if (client != NULL)
        return; // Already started

    init_identifiers();

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.username = MQTT_USER,
        .credentials.authentication.password = MQTT_PASS,
        .credentials.client_id = device_id,
        .session.last_will.topic = topic_lwt,
        .session.last_will.msg = "offline",
        .session.last_will.msg_len = 7,
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

void mqtt_helper_send_discovery(void)
{
    if (!client || !s_mqtt_connected)
        return;

    init_identifiers();

    // Temperature config
    cJSON *root_t = cJSON_CreateObject();
    cJSON_AddStringToObject(root_t, "name", SENSOR_NAME_TEMP);
    cJSON_AddStringToObject(root_t, "dev_cla", "temperature");
    cJSON_AddStringToObject(root_t, "stat_cla", "measurement");
    cJSON_AddStringToObject(root_t, "unit_of_meas", "°C");
    cJSON_AddStringToObject(root_t, "stat_t", topic_state);
    cJSON_AddStringToObject(root_t, "val_tpl", "{{ value_json.temperature }}");
    cJSON_AddStringToObject(root_t, "uniq_id", uniq_id_temp);

    cJSON_AddStringToObject(root_t, "avty_t", topic_lwt);       // availability_topic
    cJSON_AddStringToObject(root_t, "pl_avail", "online");      // payload_available
    cJSON_AddStringToObject(root_t, "pl_not_avail", "offline"); // payload_not_available

    cJSON *dev = cJSON_CreateObject();
    cJSON_AddStringToObject(dev, "ids", device_id);
    cJSON_AddStringToObject(dev, "name", device_name);
    cJSON_AddStringToObject(dev, "mf", "Espressif");
    cJSON_AddItemToObject(root_t, "dev", dev);

    char *json_str_t = cJSON_PrintUnformatted(root_t);
    esp_mqtt_client_publish(client, topic_conf_temp, json_str_t, 0, 1, 1);
    cJSON_Delete(root_t);
    free(json_str_t);

    // Humidity config
    cJSON *root_h = cJSON_CreateObject();
    cJSON_AddStringToObject(root_h, "name", SENSOR_NAME_HUM);
    cJSON_AddStringToObject(root_h, "dev_cla", "humidity");
    cJSON_AddStringToObject(root_h, "stat_cla", "measurement");
    cJSON_AddStringToObject(root_h, "unit_of_meas", "%");
    cJSON_AddStringToObject(root_h, "stat_t", topic_state);
    cJSON_AddStringToObject(root_h, "val_tpl", "{{ value_json.humidity }}");
    cJSON_AddStringToObject(root_h, "uniq_id", uniq_id_hum);

    cJSON_AddStringToObject(root_h, "avty_t", topic_lwt);
    cJSON_AddStringToObject(root_h, "pl_avail", "online");
    cJSON_AddStringToObject(root_h, "pl_not_avail", "offline");

    cJSON *dev2 = cJSON_CreateObject();
    cJSON_AddStringToObject(dev2, "ids", device_id);
    cJSON_AddItemToObject(root_h, "dev", dev2);

    char *json_str_h = cJSON_PrintUnformatted(root_h);
    esp_mqtt_client_publish(client, topic_conf_hum, json_str_h, 0, 1, 1);
    cJSON_Delete(root_h);
    free(json_str_h);

    ESP_LOGI(TAG, "Discovery sent!");
}

void mqtt_helper_send_data(float temp, float hum)
{
    if (!client || !s_mqtt_connected)
        return;

    // Build JSON manually for predictable rounding (%.1f)
    char json_str[64];

    // %.1f enforces exactly one decimal place
    snprintf(json_str, sizeof(json_str), "{\"temperature\":%.1f,\"humidity\":%.1f}", temp, hum);

    // Publish data
    esp_mqtt_client_publish(client, topic_state, json_str, 0, 1, 0);
    ESP_LOGI(TAG, "Sent data: %s", json_str);
}

bool mqtt_helper_is_connected(void)
{
    return s_mqtt_connected;
}
