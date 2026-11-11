// Early bench test of the aim servo on an Arduino Uno, before the logic
// moved to the ESP32. Uses the same fast left/right and stop bytes as the
// final firmware (see firmware/main/protocol.h).

#include <Servo.h>

Servo aimServo;

void setup() {
  aimServo.attach(9);
  Serial.begin(9600);
}

void loop() {
  if (Serial.available() > 0) {
    switch (Serial.read()) {
      case 0xFF: aimServo.write(120); break;  // right
      case 0x77: aimServo.write(60);  break;  // left
      case 0x69: aimServo.write(90);  break;  // stop
    }
  }
}
