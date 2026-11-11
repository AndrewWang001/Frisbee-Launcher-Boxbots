"""Track the nearest person with YOLO11 and steer the launcher over UART.

An open palm (MediaPipe gesture recognizer) fires a shot.
Press q to quit.
"""

import argparse
import time

import cv2
import mediapipe as mp
import serial
from ultralytics import YOLO

import protocol

PERSON_CLASS = 0
FIRE_GESTURE = "Open_Palm"


def parse_args():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", help="ESP32 serial port, e.g. COM3 or /dev/ttyUSB0. Omit to run without hardware.")
    p.add_argument("--camera", type=int, default=0)
    p.add_argument("--model", default="yolo11m.pt")
    p.add_argument("--gesture-model", default="gesture_recognizer.task")
    p.add_argument("--deadband", type=int, default=10, help="px from center where the turret stops")
    p.add_argument("--fast-band", type=int, default=80, help="px from center beyond which the turret turns fast")
    p.add_argument("--cooldown", type=float, default=5.0, help="seconds between shots")
    p.add_argument("--max-duty", type=int, help="override flywheel max duty (0-100%%)")
    return p.parse_args()


def open_serial(port):
    if port is None:
        return None
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = protocol.BAUD
    ser.timeout = 1
    # Keep DTR/RTS low so opening the port doesn't reset the ESP32.
    ser.dtr = False
    ser.rts = False
    ser.open()
    return ser


def send(ser, *data):
    if ser is not None:
        ser.write(bytes(data))


def aim_command(target_x, center_x, deadband, fast_band):
    error = target_x - center_x
    if abs(error) <= deadband:
        return protocol.AIM_STOP
    if error > 0:
        return protocol.AIM_RIGHT_FAST if error > fast_band else protocol.AIM_RIGHT_SLOW
    return protocol.AIM_LEFT_FAST if -error > fast_band else protocol.AIM_LEFT_SLOW


def largest_person(boxes):
    best, best_area = None, 0
    for x1, y1, x2, y2 in boxes:
        area = (x2 - x1) * (y2 - y1)
        if area > best_area:
            best, best_area = (x1, y1, x2, y2), area
    return best


def draw_hands(frame, hand_landmarks):
    h, w = frame.shape[:2]
    for hand in hand_landmarks:
        for lm in hand:
            cv2.circle(frame, (int(lm.x * w), int(lm.y * h)), 3, (0, 255, 0), -1)


def main():
    args = parse_args()

    cap = cv2.VideoCapture(args.camera)
    cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    if not cap.isOpened():
        raise SystemExit(f"Could not open camera {args.camera}")

    model = YOLO(args.model)
    recognizer = mp.tasks.vision.GestureRecognizer.create_from_options(
        mp.tasks.vision.GestureRecognizerOptions(
            base_options=mp.tasks.BaseOptions(model_asset_path=args.gesture_model),
            running_mode=mp.tasks.vision.RunningMode.VIDEO,
            num_hands=2,
        )
    )

    ser = open_serial(args.port)
    if args.max_duty is not None:
        send(ser, protocol.SET_MAX_DUTY, max(0, min(100, args.max_duty)))

    start = time.monotonic()
    last_shot = -args.cooldown
    palm_was_up = False

    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                print("Camera stream ended")
                break
            center_x = frame.shape[1] // 2

            # Aim at the largest (usually closest) person.
            result = model(frame, classes=[PERSON_CLASS], verbose=False)[0]
            boxes = [tuple(map(int, b)) for b in result.boxes.xyxy.tolist()]
            for x1, y1, x2, y2 in boxes:
                cv2.rectangle(frame, (x1, y1), (x2, y2), (255, 0, 255), 1)

            target = largest_person(boxes)
            if target:
                x1, y1, x2, y2 = target
                cv2.rectangle(frame, (x1, y1), (x2, y2), (255, 0, 255), 3)
                cmd = aim_command((x1 + x2) // 2, center_x, args.deadband, args.fast_band)
            else:
                cmd = protocol.AIM_STOP
            send(ser, cmd)  # sent every frame; the ESP32 stops the turret if these stop

            # Fire on the rising edge of an open palm, rate limited.
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
            timestamp_ms = int((time.monotonic() - start) * 1000)
            gestures = recognizer.recognize_for_video(mp_image, timestamp_ms)
            draw_hands(frame, gestures.hand_landmarks)

            names = [g[0].category_name for g in gestures.gestures if g]
            palm_up = FIRE_GESTURE in names
            now = time.monotonic()
            if palm_up and not palm_was_up and now - last_shot >= args.cooldown:
                send(ser, protocol.FIRE)
                last_shot = now
                print("Fire")
            palm_was_up = palm_up

            cv2.line(frame, (center_x, 0), (center_x, frame.shape[0]), (200, 200, 200), 1)
            cv2.putText(frame, " ".join(names), (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
            cv2.imshow("Launcher", frame)
            if cv2.waitKey(1) == ord("q"):
                break
    finally:
        send(ser, protocol.AIM_STOP)
        if ser is not None:
            ser.close()
        recognizer.close()
        cap.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
