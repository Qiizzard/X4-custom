#pragma once
// Bounded SHA-1 / six-digit / 30-second TOTP adapted from Biscuit (MIT).
#include <mbedtls/md.h>

#include <cstddef>
#include <cstdint>

class Totp {
 public:
  Totp() { mbedtls_md_init(&context); }
  ~Totp() { close(); }
  Totp(const Totp&) = delete;
  Totp& operator=(const Totp&) = delete;
  bool open();  // Allocate crypto context once per unlocked session, not per code.
  void close();
  static bool validSeed(const char* seed);
  bool generate(const char* seed, uint64_t unixSeconds, char (&code)[7]);

 private:
  mbedtls_md_context_t context{};
  bool ready = false;
  static bool decode(const char* seed, uint8_t (&key)[40], size_t& length);
};
