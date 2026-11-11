#include "status_led.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "pins.h"

#define HEARTBEAT_MS 500

void status_led_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_STATUS_LED) | (1ULL << PIN_FIRE_LED),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(PIN_STATUS_LED, 0);
    gpio_set_level(PIN_FIRE_LED, 0);
}

void status_led_set_firing(bool on)
{
    gpio_set_level(PIN_FIRE_LED, on);
}

// Blinks the on-board LED so it's obvious the firmware is running.
void status_led_task(void *arg)
{
    int level = 0;
    while (1) {
        level = !level;
        gpio_set_level(PIN_STATUS_LED, level);
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_MS));
    }
}
