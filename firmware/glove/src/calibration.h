#pragma once
#include <Preferences.h>
#include "flex.h"
// Serial 'o': hold open, 'b': hold bent, 's': save, 'x': erase. No automatic save.
struct GloveCalibration {
  flight::FlexCalibration flex[2];
  bool collecting = false, bent = false, editing = false;
  unsigned count = 0;
  uint32_t sums[2] = {};
  void load() {
    Preferences p;
    if (!p.begin("glove-cal", true))
      return;
    for (int i = 0; i < 2; i++) {
      char key[8];
      snprintf(key, sizeof(key), "o%d", i);
      flex[i].open = p.getFloat(key, 0);
      snprintf(key, sizeof(key), "b%d", i);
      flex[i].bent = p.getFloat(key, 0);
      flex[i].measured = p.getBool("valid", false);
    }
    p.end();
  }
  bool valid() const {
    return flex[0].valid() && flex[1].valid();
  }
  void poll(bool mayCalibrate, const uint16_t raw[2]) {
    if (Serial.available()) {
      char c = Serial.read();
      if (!mayCalibrate) {
        Serial.println("Calibration locked while command stream enabled");
        return;
      }
      if (c == 'o' || c == 'b') {
        editing = true;
        collecting = true;
        bent = c == 'b';
        count = 0;
        sums[0] = sums[1] = 0;
      }
      if (c == 'x') {
        editing = true;
        Preferences p;
        if (p.begin("glove-cal", false)) {
          bool cleared = p.clear();
          p.end();
          Serial.println(cleared ? "NVS erased" : "ERROR NVS erase failed");
        } else {
          Serial.println("ERROR NVS open failed");
        }
        flex[0] = flex[1] = {};
        Serial.println("Calibration erased");
      }
      if (c == 's') {
        if (!valid())
          Serial.println("Invalid endpoints: repeat open/bent captures");
        else {
          Preferences p;
          if (p.begin("glove-cal", false)) {
            if (p.putBool("valid", false) != 1) {
              p.end();
              Serial.println("ERROR cannot invalidate old NVS record; save aborted");
              return;
            }
            bool saved = true;
            for (int i = 0; i < 2; i++) {
              char key[8];
              snprintf(key, sizeof(key), "o%d", i);
              saved = (p.putFloat(key, flex[i].open) == sizeof(float)) && saved;
              snprintf(key, sizeof(key), "b%d", i);
              saved = (p.putFloat(key, flex[i].bent) == sizeof(float)) && saved;
            }
            if (saved)
              saved = p.putBool("valid", true) == 1;
            p.end();
            editing = !saved;
            Serial.println(saved ? "Saved; reboot and perform neutral handshake"
                                 : "ERROR NVS save failed; calibration remains inhibited");
          } else {
            Serial.println("ERROR NVS open failed");
          }
        }
      }
    }
    if (!collecting || !mayCalibrate)
      return;
    sums[0] += raw[0];
    sums[1] += raw[1];
    if (++count == 250) {
      for (int i = 0; i < 2; i++) {
        float v = sums[i] / float(count);
        if (bent)
          flex[i].bent = v;
        else
          flex[i].open = v;
        flex[i].measured = true;
      }
      collecting = false;
      Serial.printf("Endpoints open %.1f %.1f bent %.1f %.1f valid=%d\n", flex[0].open,
                    flex[1].open, flex[0].bent, flex[1].bent, valid());
    }
  }
};
