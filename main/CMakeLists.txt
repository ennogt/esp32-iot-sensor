idf_component_register(SRCS "wifi_helper.c" "main.c" "gui.c" "sensor.c" "mqtt_helper.c"
                    REQUIRES esp_wifi nvs_flash wifi_provisioning mqtt json
                    INCLUDE_DIRS ".")
