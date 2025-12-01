#pragma once

#include <stdbool.h>

void gui_init(void);
void gui_set_values(float temperature, float humidity);
void gui_set_status(const char *status_text);
