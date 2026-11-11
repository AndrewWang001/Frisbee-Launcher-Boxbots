#include "feeder.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"

#include "pins.h"
#include "flywheel.h"
#include "status_led.h"

#define SERVO_PERIOD_US  20000  // 50 Hz
#define PWM_RES_BITS     15
#define PWM_MODE         LEDC_HIGH_SPEED_MODE
#define PWM_TIMER        LEDC_TIMER_0
#define PWM_CHANNEL      LEDC_CHANNEL_0
#define STEP_MS          10

static const char *TAG = "feeder";
static TaskHandle_t s_task;

static void set_pulse_us(int us)
{
    uint32_t duty = (uint32_t)us * (1u << PWM_RES_BITS) / SERVO_PERIOD_US;
    ledc_set_duty(PWM_MODE, PWM_CHANNEL, duty);
    ledc_update_duty(PWM_MODE, PWM_CHANNEL);
}

// Sweep instead of jumping so the feeder pushes the disc smoothly.
static void sweep(int from_us, int to_us)
{
    int steps = CONFIG_FEEDER_TRAVEL_MS / STEP_MS;
    for (int i = 1; i <= steps; i++) {
        set_pulse_us(from_us + (to_us - from_us) * i / steps);
        vTaskDelay(pdMS_TO_TICKS(STEP_MS));
    }
}

void feeder_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = PWM_MODE,
        .timer_num = PWM_TIMER,
        .duty_resolution = PWM_RES_BITS,
        .freq_hz = 1000000 / SERVO_PERIOD_US,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel = {
        .gpio_num = PIN_FEEDER_SERVO,
        .speed_mode = PWM_MODE,
        .channel = PWM_CHANNEL,
        .timer_sel = PWM_TIMER,
        .duty = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
    set_pulse_us(CONFIG_FEEDER_REST_US);
}

void feeder_request_fire(void)
{
    if (s_task) {
        xTaskNotifyGive(s_task);
    }
}

// One shot per request: spin up the flywheel, push the disc into it,
// retract, spin down.
void feeder_task(void *arg)
{
    s_task = xTaskGetCurrentTaskHandle();

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "fire (max duty %d%%)", flywheel_get_max_duty_pct());

        status_led_set_firing(true);
        flywheel_start();
        vTaskDelay(pdMS_TO_TICKS(CONFIG_FIRE_SPINUP_MS));

        sweep(CONFIG_FEEDER_REST_US, CONFIG_FEEDER_PUSH_US);
        vTaskDelay(pdMS_TO_TICKS(CONFIG_FEEDER_HOLD_MS));
        sweep(CONFIG_FEEDER_PUSH_US, CONFIG_FEEDER_REST_US);

        flywheel_stop();
        status_led_set_firing(false);

        ulTaskNotifyTake(pdTRUE, 0);  // ignore requests made during the shot
    }
}
