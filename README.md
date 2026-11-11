# Vision-Guided Frisbee Launcher

Hackathon project. A laptop runs YOLO11 on a webcam feed, finds the closest person, and steers the launcher toward them over UART. Holding up an open palm fires. The launcher is an ESP32 driving a flywheel and two servos, built from cardboard and 3D-printed parts.

```
firmware/   ESP32 firmware (C, ESP-IDF, FreeRTOS)
vision/     Tracking and gesture detection (Python, OpenCV, YOLO11, MediaPipe)
legacy/     Arduino sketch used to bench-test the aim servo early on
```

## Firmware

Five FreeRTOS tasks:

| Task | Job |
|---|---|
| `aim` | Reads UART commands, drives the continuous-rotation aim servo (MCPWM, 50 Hz) |
| `feeder` | Runs a shot: spin up flywheel, push disc in, retract, spin down (LEDC, 50 Hz) |
| `flywheel` | Ramps the motor PWM toward its target (LEDC, 20 kHz, 10-bit, through an L298N) |
| `button` | Debounced fire button (GPIO interrupt) |
| `status_led` | Heartbeat LED |

Flywheel max duty, ramp time, feeder positions and aim speeds are set under `idf.py menuconfig` > Frisbee launcher. Max duty can also be changed at runtime over UART.

| Pin | Use |
|---|---|
| 13 | Aim servo |
| 25 | Feeder servo |
| 21 / 19 / 18 | L298N IN1 (PWM) / IN2 / ENA |
| 4 | Fire button (to GND) |
| 15 | Fire LED |
| 2 | Status LED |

Build and flash (ESP-IDF 5.x):

```
cd firmware
idf.py build flash monitor
```

## Vision

```
cd vision
pip install -r requirements.txt
curl -LO https://storage.googleapis.com/mediapipe-models/gesture_recognizer/gesture_recognizer/float16/latest/gesture_recognizer.task
python tracker.py --port COM3
```

The YOLO weights download on first run. Leave off `--port` to try it without the hardware. `serial_test.py` runs the turret through each command without the camera.

## UART protocol

9600 8N1 over the ESP32's USB port. One byte per command:

| Byte | Command |
|---|---|
| `0x01` | Fire |
| `0x02 N` | Set flywheel max duty to N% |
| `0xFF` / `0x30` | Turn right, fast / slow |
| `0x77` / `0x20` | Turn left, fast / slow |
| `0x69` | Stop |

The tracker sends an aim command every frame. If they stop for more than a second the ESP32 stops the turret.
