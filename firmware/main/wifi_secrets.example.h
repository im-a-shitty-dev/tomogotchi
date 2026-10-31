// Copy to wifi_secrets.h (gitignored) and fill in. Without it the firmware
// still builds: no clock sync, but tube detection still works.
#pragma once

#define TOMO_WIFI_SSID "your-home-network"
#define TOMO_WIFI_PASSWORD "your-password"

// Optional: the network that means "home", if it's not the one above.
// #define TOMO_HOME_SSID "your-home-network"
