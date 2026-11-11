"""Exercise the ESP32 over UART without the camera.

Turns the turret left and right at both speeds, then stops. Pass --fire to
also fire one shot at the end.
"""

import argparse
import time

import serial

import protocol

SEQUENCE = [
    ("right fast", protocol.AIM_RIGHT_FAST),
    ("left fast", protocol.AIM_LEFT_FAST),
    ("right slow", protocol.AIM_RIGHT_SLOW),
    ("left slow", protocol.AIM_LEFT_SLOW),
]


def hold(ser, cmd, seconds):
    # Resend while holding, since the firmware stops the turret if commands stop.
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        ser.write(bytes([cmd]))
        time.sleep(0.1)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("port", help="e.g. COM3 or /dev/ttyUSB0")
    p.add_argument("--fire", action="store_true")
    args = p.parse_args()

    ser = serial.Serial()
    ser.port = args.port
    ser.baudrate = protocol.BAUD
    ser.dtr = False
    ser.rts = False
    ser.open()

    with ser:
        for name, cmd in SEQUENCE:
            print(name)
            hold(ser, cmd, 1.0)
        print("stop")
        ser.write(bytes([protocol.AIM_STOP]))

        if args.fire:
            print("fire")
            ser.write(bytes([protocol.FIRE]))


if __name__ == "__main__":
    main()
