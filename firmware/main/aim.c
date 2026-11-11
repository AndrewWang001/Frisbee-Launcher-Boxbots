#include "aim.h"

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/mcpwm_prelude.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "pins.h"
#include "protocol.h"
#include "flywheel.h"
#include "feeder.h"

// The aim servo is continuous rotation: 1500 us is stop, and the offset
// from 1500 sets speed and direction.
#define SERVO_STOP_US    1500
#define SERVO_PERIOD_US  20000  // 50 Hz

#define UART_PORT        UART_NUM_0  // USB-serial port, shared with log output
#define UART_BAUD        9600
#define UART_RX_BUF      256
#define UART_POLL_MS     20

static const char *TAG = "aim";
static mcpwm_cmpr_handle_t s_cmpr;

static void set_speed_us(int offset_us)
{
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(s_cmpr, SERVO_STOP_US + offset_us));
}

static void servo_init(void)
{
    mcpwm_timer_handle_t timer;
    mcpwm_timer_config_t timer_cfg = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000,  // 1 tick = 1 us
        .period_ticks = SERVO_PERIOD_US,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_cfg, &timer));

    mcpwm_oper_handle_t oper;
    mcpwm_operator_config_t oper_cfg = { .group_id = 0 };
    ESP_ERROR_CHECK(mcpwm_new_operator(&oper_cfg, &oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    mcpwm_comparator_config_t cmpr_cfg = { .flags.update_cmp_on_tez = true };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &cmpr_cfg, &s_cmpr));

    mcpwm_gen_handle_t gen;
    mcpwm_generator_config_t gen_cfg = { .gen_gpio_num = PIN_AIM_SERVO };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &gen_cfg, &gen));

    set_speed_us(0);

    // High at the start of each period, low when the counter hits the compare value.
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(gen,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gen,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, s_cmpr, MCPWM_GEN_ACTION_LOW)));

    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
}

static void uart_init(void)
{
    uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_RX_BUF, 0, 0, NULL, 0));
}

void aim_init(void)
{
    servo_init();
    uart_init();
}

// Maps an aim command to a servo speed. Returns false if it isn't one.
static bool aim_offset_for(uint8_t cmd, int *offset_us)
{
    switch (cmd) {
    case CMD_AIM_RIGHT_FAST: *offset_us =  CONFIG_AIM_FAST_OFFSET_US; return true;
    case CMD_AIM_RIGHT_SLOW: *offset_us =  CONFIG_AIM_SLOW_OFFSET_US; return true;
    case CMD_AIM_LEFT_FAST:  *offset_us = -CONFIG_AIM_FAST_OFFSET_US; return true;
    case CMD_AIM_LEFT_SLOW:  *offset_us = -CONFIG_AIM_SLOW_OFFSET_US; return true;
    case CMD_AIM_STOP:       *offset_us = 0;                          return true;
    default:                 return false;
    }
}

// Reads commands from the vision host. The host sends an aim command every
// frame; if they stop arriving the turret is stopped so it can't keep spinning.
void aim_task(void *arg)
{
    bool expect_duty_byte = false;
    int current_offset = 0;
    TickType_t last_cmd = xTaskGetTickCount();

    while (1) {
        uint8_t b;
        int len = uart_read_bytes(UART_PORT, &b, 1, pdMS_TO_TICKS(UART_POLL_MS));

        if (len <= 0) {
            if (current_offset != 0 &&
                xTaskGetTickCount() - last_cmd > pdMS_TO_TICKS(CONFIG_AIM_TIMEOUT_MS)) {
                ESP_LOGW(TAG, "no commands, stopping");
                current_offset = 0;
                set_speed_us(0);
            }
            continue;
        }
        last_cmd = xTaskGetTickCount();

        if (expect_duty_byte) {
            expect_duty_byte = false;
            flywheel_set_max_duty_pct(b);
            continue;
        }

        int offset;
        if (aim_offset_for(b, &offset)) {
            if (offset != current_offset) {
                current_offset = offset;
                set_speed_us(offset);
                ESP_LOGD(TAG, "speed %+d us", offset);
            }
        } else if (b == CMD_FIRE) {
            feeder_request_fire();
        } else if (b == CMD_SET_MAX_DUTY) {
            expect_duty_byte = true;
        } else {
            ESP_LOGW(TAG, "unknown command 0x%02X", b);
        }
    }
}
