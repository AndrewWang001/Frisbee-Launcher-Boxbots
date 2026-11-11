#include "button.h"

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "pins.h"
#include "feeder.h"

#define DEBOUNCE_MS 30

static const char *TAG = "button";
static QueueHandle_t s_events;

static void IRAM_ATTR button_isr(void *arg)
{
    uint32_t pin = (uint32_t)arg;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(s_events, &pin, &woken);
    portYIELD_FROM_ISR(woken);
}

void button_init(void)
{
    s_events = xQueueCreate(8, sizeof(uint32_t));

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_FIRE_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_FIRE_BUTTON, button_isr, (void *)PIN_FIRE_BUTTON));
}

// Debounces the fire button and turns a press into a fire request.
void button_task(void *arg)
{
    uint32_t pin;
    while (1) {
        xQueueReceive(s_events, &pin, portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
        if (gpio_get_level(PIN_FIRE_BUTTON) != 0) {
            continue;  // bounce or noise
        }

        ESP_LOGI(TAG, "pressed");
        feeder_request_fire();

        while (gpio_get_level(PIN_FIRE_BUTTON) == 0) {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        xQueueReset(s_events);  // drop edges from bounce
    }
}
