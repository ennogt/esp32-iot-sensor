#pragma once
#include <stdbool.h>

void gui_init(void);
void gui_set_values(float temperature, float humidity);
void gui_set_status(const char *status_text);
void gui_turn_off(void);
void gui_turn_on(void);
bool gui_is_enabled(void);
