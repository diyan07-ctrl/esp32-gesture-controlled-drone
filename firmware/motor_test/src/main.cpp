#include <Arduino.h>
#include "esc.h"
// Deliberately separate image: no RF, no stabilization, never a flight controller.
flight::Esc esc;
bool enabled = false;
int selected = -1;
uint32_t lastCommand = 0;
void setup() {
  Serial.begin(115200);
  esc.safeBoot();
  pinMode(cfg::kill, INPUT_PULLUP);
  Serial.println("PROPS OFF motor fixture only. Send ! to initialize; 1..4 gives 5% for <=1000ms; "
                 "x stops. All output gates still apply.");
}
void loop() {
  uint32_t now = millis();
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '!' && !enabled) {
      enabled = esc.attach();
      Serial.printf("ESC test enabled=%d\n", enabled);
    }
    if (c >= '1' && c <= '4' && enabled) {
      selected = c - '1';
      lastCommand = now;
    }
    if (c == 'x') {
      selected = -1;
    }
  }
  if (uint32_t(now - lastCommand) > 1000 ||
      (cfg::localPermitInstalled && digitalRead(cfg::kill) == HIGH))
    selected = -1;
  flight::Mix m = flight::mix(0, 0, 0, 0);
  if (selected >= 0)
    m.motor[selected] = .05f;
  esc.write(m, enabled && selected >= 0);
  delay(1);
}
