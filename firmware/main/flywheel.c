#include "flywheel.h"

#include <stdatomic.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

#include "pins.h"

#define PWM_FREQ_HZ   20000
#define PWM_RES       LEDC_TIMER_10_BIT
#define DUTY_FULL     ((1 << 10) - 1)
#define PWM_MODE      LEDC_LOW_SPEED_MODE
#define PWM_TIMER     LEDC_TIMER_1
#define PWM_CHANNEL   LEDC_CHANNEL_1
#define TICK_MS       10

static const char *TAG = "flywheel";

static atomic_bool s_running;
static atomic_int  s_max_duty_pct = CONFIG_FLYWHEEL_MAX_DUTY_PCT;

void flywheel_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_FLYWHEEL_ENA) | (1ULL << PIN_FLYWHEEL_IN2),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(PIN_FLYWHEEL_ENA, 1);
    gpio_set_level(PIN_FLYWHEEL_IN2, 0);

    ledc_timer_config_t timer = {
        .speed_mode = PWM_MODE,
        .timer_num = PWM_TIMER,
        .duty_resolution = PWM_RES,
        .freq_hz = PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel = {
        .gpio_num = PIN_FLYWHEEL_IN1,
        .speed_mode = PWM_MODE,
        .channel = PWM_CHANNEL,
        .timer_sel = PWM_TIMER,
        .duty = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

void flywheel_start(void) { atomic_store(&s_running, true); }
void flywheel_stop(void)  { atomic_store(&s_running, false); }

void flywheel_set_max_duty_pct(int pct)
{
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    atomic_store(&s_max_duty_pct, pct);
    ESP_LOGI(TAG, "max duty set to %d%%", pct);
}

int flywheel_get_max_duty_pct(void)
{
    return atomic_load(&s_max_duty_pct);
}

// Ramps the motor PWM toward the target so the flywheel doesn't jerk or
// brown out the supply. The step is sized so 0 -> max duty takes
// CONFIG_FLYWHEEL_RAMP_MS.
void flywheel_task(void *arg)
{
    int duty = 0;

    while (1) {
        int max_duty = DUTY_FULL * flywheel_get_max_duty_pct() / 100;
        int target = atomic_load(&s_running) ? max_duty : 0;

        int step = max_duty * TICK_MS / CONFIG_FLYWHEEL_RAMP_MS;
        if (step < 1) step = 1;

        if (duty < target) {
            duty = (duty + step > target) ? target : duty + step;
        } else if (duty > target) {
            duty = (duty - step < target) ? target : duty - step;
        }

        ledc_set_duty(PWM_MODE, PWM_CHANNEL, duty);
        ledc_update_duty(PWM_MODE, PWM_CHANNEL);

        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}
