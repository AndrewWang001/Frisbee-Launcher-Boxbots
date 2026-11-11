#pragma once

// Single-byte UART commands from the vision host (9600 8N1, UART0).
// Keep in sync with vision/protocol.py.

#define CMD_FIRE            0x01
#define CMD_SET_MAX_DUTY    0x02  // followed by one byte: flywheel max duty, 0-100 (%)

#define CMD_AIM_LEFT_SLOW   0x20
#define CMD_AIM_RIGHT_SLOW  0x30
#define CMD_AIM_STOP        0x69
#define CMD_AIM_LEFT_FAST   0x77
#define CMD_AIM_RIGHT_FAST  0xFF
