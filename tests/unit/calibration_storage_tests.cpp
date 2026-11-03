#include <iostream>
#include <Arduino.h>
#include "../../firmware/glove/src/calibration.h"
unsigned checks = 0;
#define CHECK(x)                                                                                   \
  do {                                                                                             \
    ++checks;                                                                                      \
    if (!(x)) {                                                                                    \
      std::cerr << "FAIL " << __LINE__ << " " << #x << '\n';                                       \
      return 1;                                                                                    \
    }                                                                                              \
  } while (0)
int main() {
  GloveCalibration c;
  c.load();
  CHECK(!c.valid());
  const uint16_t open[2] = {1000, 1200}, bent[2] = {3000, 2800};
  Serial.input = "o";
  c.poll(false, open);
  CHECK(!c.collecting && !c.editing);
  Serial.input = "o";
  for (int i = 0; i < 250; i++)
    c.poll(true, open);
  CHECK(c.editing && !c.collecting);
  CHECK(c.flex[0].open == 1000 && c.flex[1].open == 1200);
  CHECK(!c.valid());
  Serial.input = "b";
  for (int i = 0; i < 250; i++)
    c.poll(true, bent);
  CHECK(c.valid());
  CHECK(c.editing);
  Serial.input = "s";
  c.poll(true, bent);
  CHECK(!c.editing);
  GloveCalibration loaded;
  loaded.load();
  CHECK(loaded.valid());
  CHECK(loaded.flex[1].bent == 2800);
  // Power/write failure before commit cannot expose a mixed valid NVS record.
  Serial.input = "o";
  for (int i = 0; i < 250; i++)
    c.poll(true, open);
  mock_nvs::writes = 0;
  mock_nvs::failAt = 2;
  Serial.input = "s";
  c.poll(true, open);
  CHECK(c.editing);
  GloveCalibration partial;
  partial.load();
  CHECK(!partial.valid());
  mock_nvs::failAt = 0;
  Serial.input = "s";
  c.poll(true, open);
  CHECK(!c.editing);
  loaded.load();
  CHECK(loaded.valid());
  mock_nvs::openFail = true;
  Serial.input = "o";
  c.poll(true, open);
  Serial.input = "s";
  c.poll(true, open);
  CHECK(c.editing);
  mock_nvs::openFail = false;
  Serial.input = "x";
  c.poll(true, open);
  CHECK(c.editing && !c.valid());
  loaded.load();
  CHECK(!loaded.valid());
  std::cout << "PASS " << checks
            << " mocked serial/NVS calibration transaction assertions (no flash hardware)\n";
}
