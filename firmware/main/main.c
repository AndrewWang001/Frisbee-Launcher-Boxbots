#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "status_led.h"
#include "button.h"
#include "flywheel.h"
#include "feeder.h"
#include "aim.h"

void app_main(void)
{
    status_led_init();
    flywheel_init();
    feeder_init();
    aim_init();
    button_init();

    // Higher number = higher priority. Aim and feeder timing matter most.
    xTaskCreate(status_led_task, "status_led", 2048, NULL, 2, NULL);
    xTaskCreate(button_task,     "button",     3072, NULL, 3, NULL);
    xTaskCreate(flywheel_task,   "flywheel",   3072, NULL, 5, NULL);
    xTaskCreate(feeder_task,     "feeder",     3072, NULL, 6, NULL);
    xTaskCreate(aim_task,        "aim",        3072, NULL, 7, NULL);
}
