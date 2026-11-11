#pragma once

#include "driver/gpio.h"

#define PIN_STATUS_LED    GPIO_NUM_2   // on-board LED, heartbeat
#define PIN_FIRE_LED      GPIO_NUM_15  // on while a shot is in progress
#define PIN_FIRE_BUTTON   GPIO_NUM_4   // active low, internal pull-up

#define PIN_FLYWHEEL_IN1  GPIO_NUM_21  // L298N IN1 (PWM)
#define PIN_FLYWHEEL_IN2  GPIO_NUM_19  // L298N IN2 (held low, fixed direction)
#define PIN_FLYWHEEL_ENA  GPIO_NUM_18  // L298N ENA (remove the ENA jumper)

#define PIN_FEEDER_SERVO  GPIO_NUM_25
#define PIN_AIM_SERVO     GPIO_NUM_13
