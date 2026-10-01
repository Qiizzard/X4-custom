#pragma once
#include <cstddef>

// Wi-Fi QR text, with bounded SSID/key and escaped delimiter characters.
// auth: 0 WPA/WPA2, 1 WEP, 2 open (does not read/encode the password).
// Output is cleared on failure; caller owns and wipes successful secret text.
bool buildWifiQrPayload(const char* ssid, const char* password, unsigned auth, char* output, size_t capacity);
