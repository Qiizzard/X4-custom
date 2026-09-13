#include "Totp.h"

#include <Logging.h>
#include <SecureStore.h>

#include <cstdio>
#include <cstring>

bool Totp::decode(const char* seed, uint8_t (&key)[40], size_t& length) {
  length = 0;
  memset(key, 0, sizeof(key));
  if (!seed) return false;
  const size_t size = strnlen(seed, 64);
  // Unpadded canonical Base32. Reject junk rather than silently changing a key.
  if (size < 16 || size > 63) return false;
  const size_t remainder = size % 8;
  if (remainder == 1 || remainder == 3 || remainder == 6) return false;
  uint32_t accumulator = 0;
  unsigned bits = 0;
  for (size_t i = 0; i < size; ++i) {
    char c = seed[i];
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    unsigned value;
    if (c >= 'A' && c <= 'Z')
      value = c - 'A';
    else if (c >= '2' && c <= '7')
      value = c - '2' + 26;
    else
      return false;
    accumulator = (accumulator << 5) | value;
    bits += 5;
    if (bits >= 8) {
      bits -= 8;
      key[length++] = static_cast<uint8_t>(accumulator >> bits);
      accumulator &= (1u << bits) - 1;
    }
  }
  return accumulator == 0 && length >= 10;
}
bool Totp::validSeed(const char* seed) {
  uint8_t key[40];
  size_t length = 0;
  const bool valid = decode(seed, key, length);
  securestore::secureZero(key, sizeof(key));
  return valid;
}
bool Totp::open() {
  if (ready) return true;
  const auto* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
  if (!info || mbedtls_md_setup(&context, info, 1) != 0) {
    LOG_ERR("TOTP", "SHA-1 context allocation failed");
    close();
    return false;
  }
  ready = true;
  return true;
}
void Totp::close() {
  mbedtls_md_free(&context);  // mbedTLS zeroizes HMAC pads and digest context
  mbedtls_md_init(&context);
  ready = false;
}
bool Totp::generate(const char* seed, uint64_t unixSeconds, char (&code)[7]) {
  securestore::secureZero(code, sizeof(code));
  uint8_t key[40], message[8]{}, digest[20]{};
  size_t length = 0;
  bool ok = ready && decode(seed, key, length);
  uint64_t counter = unixSeconds / 30;
  for (int i = 7; i >= 0; --i) {
    message[i] = counter & 0xff;
    counter >>= 8;
  }
  if (ok) ok = mbedtls_md_hmac_starts(&context, key, length) == 0;
  if (ok) ok = mbedtls_md_hmac_update(&context, message, sizeof(message)) == 0;
  if (ok) ok = mbedtls_md_hmac_finish(&context, digest) == 0;
  if (ok) {
    const unsigned offset = digest[19] & 15;
    const uint32_t binary = (uint32_t(digest[offset] & 127) << 24) | (uint32_t(digest[offset + 1]) << 16) |
                            (uint32_t(digest[offset + 2]) << 8) | digest[offset + 3];
    snprintf(code, sizeof(code), "%06lu", static_cast<unsigned long>(binary % 1000000));
  } else
    LOG_ERR("TOTP", "Code generation failed");
  securestore::secureZero(key, sizeof(key));
  securestore::secureZero(message, sizeof(message));
  securestore::secureZero(digest, sizeof(digest));
  return ok;
}
