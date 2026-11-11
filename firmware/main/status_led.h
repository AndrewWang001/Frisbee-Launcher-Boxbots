#pragma once

#include <stdbool.h>

void status_led_init(void);
void status_led_set_firing(bool on);
void status_led_task(void *arg);
