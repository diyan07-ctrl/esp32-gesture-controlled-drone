#pragma once
// Copy to local_wifi.h in this directory. That file is Git-ignored.
// Use a 2.4 GHz AP/hotspot pinned to cfg::radioChannel (channel 1 by default).
namespace local_wifi {
constexpr bool configured = false;
constexpr char ssid[] = "YOUR_2_4_GHZ_SSID";
constexpr char password[] = "YOUR_PASSWORD";
constexpr char destination[] = "192.168.1.10"; // Laptop address on the same network.
constexpr uint16_t port = 14550;
}
