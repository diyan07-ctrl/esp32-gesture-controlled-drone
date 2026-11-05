#pragma once
#include <Preferences.h>
#include "flex.h"
#include "potentiometer.h"
// o/b: fingers open/bent; l/h: pot low/high; s: explicit save; x: erase.
struct GloveCalibration {
  flight::FlexCalibration flex[2];
  flight::PotCalibration pot;
  bool collecting = false, editing = false;
  char mode = 0;
  flight::EndpointCapture captures[2];
  void load() {
    flex[0] = flex[1] = {};
    pot = {};
    Preferences p;
    if (!p.begin("glove-sens-v1", true))
      return;
    bool committed = p.getBool("valid", false) && p.getFloat("schema", 0) == 1;
    for (int i = 0; i < 2; i++) {
      char key[8];
      snprintf(key, sizeof(key), "o%d", i);
      flex[i].open = p.getFloat(key, 0);
      snprintf(key, sizeof(key), "b%d", i);
      flex[i].bent = p.getFloat(key, 0);
      flex[i].measured = committed;
    }
    pot = {p.getFloat("low", 0), p.getFloat("high", 0), committed};
    p.end();
  }
  bool valid() const {
    return flex[0].valid() && flex[1].valid() && pot.valid();
  }
  void poll(bool mayCalibrate, const uint16_t raw[2], uint16_t potRaw) {
    if (!mayCalibrate && collecting) {
      collecting = false;
      editing = true;
    }
    if (Serial.available()) {
      char c = Serial.read();
      if (!mayCalibrate) {
        Serial.println("Calibration locked while armed or receiver not safely disarmed");
        return;
      }
      if (c == 'o' || c == 'b' || c == 'l' || c == 'h') {
        editing = collecting = true;
        mode = c;
        captures[0] = captures[1] = {};
      }
      if (c == 'x') {
        editing = true;
        collecting = false;
        flex[0] = flex[1] = {};
        pot = {};
        Preferences p;
        if (p.begin("glove-sens-v1", false)) {
          bool cleared = p.clear();
          p.end();
          Serial.println(cleared ? "Calibration erased" : "ERROR NVS erase failed");
        } else
          Serial.println("ERROR NVS open failed");
      }
      if (c == 's') {
        if (collecting || !valid())
          Serial.println("Invalid/incomplete endpoints: capture o,b,l,h before save");
        else {
          Preferences p;
          if (!p.begin("glove-sens-v1", false)) {
            editing = true;
            Serial.println("ERROR NVS open failed");
            return;
          }
          // Commit marker last. An interrupted write cannot enable mixed endpoints.
          if (p.putBool("valid", false) != 1) {
            p.end();
            editing = true;
            Serial.println("ERROR cannot invalidate old NVS record; save aborted");
            return;
          }
          bool saved = p.putFloat("schema", 1) == sizeof(float);
          for (int i = 0; i < 2; i++) {
            char key[8];
            snprintf(key, sizeof(key), "o%d", i);
            saved = (p.putFloat(key, flex[i].open) == sizeof(float)) && saved;
            snprintf(key, sizeof(key), "b%d", i);
            saved = (p.putFloat(key, flex[i].bent) == sizeof(float)) && saved;
          }
          saved = (p.putFloat("low", pot.low) == sizeof(float)) && saved;
          saved = (p.putFloat("high", pot.high) == sizeof(float)) && saved;
          if (saved)
            saved = p.putBool("valid", true) == 1;
          p.end();
          editing = !saved;
          Serial.println(saved ? "Saved; re-establish low throttle and neutral"
                               : "ERROR NVS save failed; commands inhibited");
        }
      }
    }
    if (!collecting || !mayCalibrate)
      return;
    bool fingers = mode == 'o' || mode == 'b';
    bool accepted = captures[0].add(fingers ? raw[0] : potRaw);
    if (fingers)
      accepted = captures[1].add(raw[1]) && accepted;
    if (!accepted) {
      collecting = false;
      Serial.println("ERROR invalid ADC capture");
      return;
    }
    if (captures[0].count == cfg::endpointSamples) {
      collecting = false;
      if (!captures[0].valid() || (fingers && !captures[1].valid())) {
        Serial.println("ERROR noisy/rail endpoint: repeat capture");
        return;
      }
      if (fingers) {
        for (int i = 0; i < 2; i++) {
          if (mode == 'b')
            flex[i].bent = captures[i].average();
          else
            flex[i].open = captures[i].average();
          flex[i].measured = true;
        }
      } else {
        if (mode == 'l')
          pot.low = captures[0].average();
        else
          pot.high = captures[0].average();
        pot.measured = true;
      }
      Serial.printf("CAL %c open=%.1f/%.1f bent=%.1f/%.1f pot=%.1f/%.1f valid=%d\n", mode,
                    flex[0].open, flex[1].open, flex[0].bent, flex[1].bent, pot.low, pot.high,
                    valid());
    }
  }
};
