#pragma once
#include <stdbool.h>

void sensor_init(void);
bool sensor_read_values(float *temperature, float *humidity);
