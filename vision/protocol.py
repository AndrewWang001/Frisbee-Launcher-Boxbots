"""UART commands understood by the ESP32. Keep in sync with firmware/main/protocol.h."""

BAUD = 9600

FIRE = 0x01
SET_MAX_DUTY = 0x02  # followed by one byte, 0-100 (%)

AIM_LEFT_SLOW = 0x20
AIM_RIGHT_SLOW = 0x30
AIM_STOP = 0x69
AIM_LEFT_FAST = 0x77
AIM_RIGHT_FAST = 0xFF
