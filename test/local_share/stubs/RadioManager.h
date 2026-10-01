#pragma once
#include <cstdint>
#include <cstring>
struct TestRadio {
  bool accessPointAddress(const char* owner, uint8_t* out) {
    if (strcmp(owner, "test_owner")) return false;
    out[0] = 127;
    out[1] = 0;
    out[2] = 0;
    out[3] = 1;
    return true;
  }
};
inline TestRadio RADIO;
