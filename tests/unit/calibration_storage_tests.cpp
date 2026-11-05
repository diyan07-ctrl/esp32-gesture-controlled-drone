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
  c.poll(false, open, 1000);
  CHECK(!c.collecting && !c.editing);
  Serial.input = "o";
  for (int i = 0; i < 250; i++)
    c.poll(true, open, 1000);
  CHECK(c.editing && !c.collecting);
  CHECK(c.flex[0].open == 1000 && c.flex[1].open == 1200);
  CHECK(!c.valid());
  Serial.input = "b";
  for (int i = 0; i < 250; i++)
    c.poll(true, bent, 1000);
  CHECK(!c.valid()); // Flex-only legacy calibration must never enable pot control.
  Serial.input = "l";
  for (int i = 0; i < 250; i++)
    c.poll(true, open, 1000);
  Serial.input = "h";
  for (int i = 0; i < 250; i++)
    c.poll(true, open, 3000);
  CHECK(c.valid());
  CHECK(c.editing);
  Serial.input = "s";
  c.poll(true, bent, 1000);
  CHECK(!c.editing);
  GloveCalibration loaded;
  loaded.load();
  CHECK(loaded.valid());
  CHECK(loaded.flex[1].bent == 2800);
  // Power/write failure before commit cannot expose a mixed valid NVS record.
  Serial.input = "o";
  for (int i = 0; i < 250; i++)
    c.poll(true, open, 1000);
  mock_nvs::writes = 0;
  mock_nvs::failAt = 2;
  Serial.input = "s";
  c.poll(true, open, 1000);
  CHECK(c.editing);
  GloveCalibration partial;
  partial.load();
  CHECK(!partial.valid());
  mock_nvs::failAt = 0;
  Serial.input = "s";
  c.poll(true, open, 1000);
  CHECK(!c.editing);
  loaded.load();
  CHECK(loaded.valid());
  mock_nvs::openFail = true;
  Serial.input = "o";
  c.poll(true, open, 1000);
  Serial.input = "s";
  c.poll(true, open, 1000);
  CHECK(c.editing);
  mock_nvs::openFail = false;
  Serial.input = "x";
  c.poll(true, open, 1000);
  CHECK(c.editing && !c.valid());
  loaded.load();
  CHECK(!loaded.valid());
  // Noisy captures do not update an endpoint, save cannot interrupt capture,
  // and losing calibration permission cancels collection.
  Serial.input = "l";
  for (int i = 0; i < 250; i++)
    c.poll(true, open, uint16_t(i % 2 ? 1000 : 1100));
  CHECK(!c.pot.measured && c.editing && !c.collecting);
  Serial.input = "l";
  c.poll(true, open, 1000);
  Serial.input = "s";
  c.poll(true, open, 1000);
  CHECK(c.collecting && c.editing);
  c.poll(false, open, 1000);
  CHECK(!c.collecting && c.editing);
  // Missing/wrong schema rejects legacy or incompatible NVS data.
  mock_nvs::values["valid"] = 1;
  mock_nvs::values["schema"] = 2;
  loaded.load();
  CHECK(!loaded.valid());
  // Inject failure at every write, including the final commit marker. A failure
  // to invalidate keeps the old complete record; all later failures load invalid.
  for (unsigned failure = 1; failure <= 9; failure++) {
    mock_nvs::values = {{"valid", 1}, {"schema", 1}, {"o0", 1000},  {"b0", 3000},
                        {"o1", 1200}, {"b1", 2800},  {"low", 1000}, {"high", 3000}};
    GloveCalibration trial;
    trial.load();
    CHECK(trial.valid());
    trial.flex[0].open = 1100;
    trial.editing = true;
    mock_nvs::writes = 0;
    mock_nvs::failAt = failure;
    Serial.input = "s";
    trial.poll(true, open, 1000);
    CHECK(trial.editing);
    GloveCalibration after;
    after.load();
    CHECK(failure == 1 ? after.valid() && after.flex[0].open == 1000 : !after.valid());
  }
  mock_nvs::failAt = 0;
  std::cout << "PASS " << checks
            << " mocked serial/NVS calibration transaction assertions (no flash hardware)\n";
}
