#pragma once
#include <cstdint>
namespace cfg {
constexpr int sda=21, scl=22, battery=34;
constexpr int motors[4]={25,26,27,32};
constexpr int arm=18, kill=19, led=23, throttle=34, yaw=35;
constexpr uint32_t periodUs=4000, timeoutMs=200, armHoldMs=1000;
constexpr float maxTilt=20, maxRate=90, maxYaw=60;
constexpr float angleKp=4.0f;
// Never enable by merely changing this flag. See docs/output-enablement.md.
constexpr bool outputsVerified=false;
constexpr bool powerVerified=false;
constexpr bool propulsionVerified=false;
constexpr bool controllerValidated=false;
constexpr int pwmHz=50, pwmMin=1000, pwmMax=2000, pwmBits=16;
constexpr float dividerRatio=5.7f, batteryScale=1.0f;
constexpr float lowBattery=10.8f, criticalBattery=9.9f;
// Thresholds above are bench placeholders; loaded pack thresholds need characterization.
constexpr unsigned logHz=5;
}
