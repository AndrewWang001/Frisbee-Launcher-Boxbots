#pragma once

void flywheel_init(void);
void flywheel_task(void *arg);

void flywheel_start(void);
void flywheel_stop(void);
void flywheel_set_max_duty_pct(int pct);
int  flywheel_get_max_duty_pct(void);
