#include "WifiQrPayload.h"

#include <SecureStore.h>

#include <cstring>

bool buildWifiQrPayload(const char* ssid, const char* password, unsigned auth, char* output, size_t capacity) {
  if (!output || !capacity) return false;
  securestore::secureZero(output, capacity);
  if (!ssid || !ssid[0] || strnlen(ssid, 33) > 32 || auth > 2 ||
      (auth != 2 && (!password || !password[0] || strnlen(password, 64) > 63)))
    return false;
  size_t length = 0;
  auto append = [&](const char* text, bool escape) {
    while (*text) {
      const char c = *text++;
      const bool special = escape && (c == '\\' || c == ';' || c == ',' || c == '"' || c == ':');
      const size_t needed = special ? 2 : 1;
      if (needed >= capacity - length) return false;  // Also reserve terminating NUL.
      if (special) output[length++] = '\\';
      output[length++] = c;
    }
    output[length] = 0;
    return true;
  };
  const bool ok = append("WIFI:T:", false) &&
                  append(auth == 0   ? "WPA"
                         : auth == 1 ? "WEP"
                                     : "nopass",
                         false) &&
                  append(";S:", false) && append(ssid, true) && append(";P:", false) &&
                  (auth == 2 || append(password, true)) && append(";;", false);
  if (!ok) securestore::secureZero(output, capacity);
  return ok;
}
