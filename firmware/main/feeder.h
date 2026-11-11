#pragma once

void feeder_init(void);
void feeder_task(void *arg);

// Safe to call from any task. Requests that arrive mid-shot are dropped.
void feeder_request_fire(void);
