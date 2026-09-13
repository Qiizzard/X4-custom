// Trailer-carrier approach adapted from Biscuit SteganographyActivity (MIT,
// Copyright (c) 2025 Dave Allie). Plaintext/XOR checksum replaced with AEAD.
#include "EncryptedBmp.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>

namespace stegobmp {
namespace {
constexpr uint32_t kMaxCarrier = 4 * 1024 * 1024;
struct Wipe {
  void* data;
  size_t size;
  ~Wipe() {
    if (data) securestore::secureZero(data, size);
  }
};
uint32_t get32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
bool fail() {
  LOG_ERR("StegoBmp", "Encrypted BMP operation failed");
  return false;
}
bool pathIn(const char* path, const char* prefix) {
  if (!path || strncmp(path, prefix, strlen(prefix))) return false;
  const char* name = path + strlen(prefix);
  const size_t n = strnlen(name, 65);
  if (n < 5 || n > 64 || name[0] == '.') return false;
  for (size_t i = 0; i < n; ++i) {
    const char c = name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
          c == '.'))
      return false;
  }
  return strcmp(name + n - 4, ".bmp") == 0 || strcmp(name + n - 4, ".BMP") == 0;
}
bool keyValid(const char* key) { return key && strnlen(key, 65) >= 8 && strnlen(key, 65) <= 64; }
bool header(HalFile& file, uint32_t& end) {
  uint8_t bytes[54];
  if (file.read(bytes, sizeof(bytes)) != sizeof(bytes) || bytes[0] != 'B' || bytes[1] != 'M') return false;
  end = get32(bytes + 2);
  const uint32_t offset = get32(bytes + 10), dib = get32(bytes + 14);
  // Envelope bounds only, not a complete BMP decoder/validity certificate.
  return end >= 54 && end <= kMaxCarrier && end <= file.fileSize64() && offset >= 54 && offset <= end && dib >= 40 &&
         dib <= offset - 14;
}
}  // namespace
bool hide(const char* source, const char* destination, const char* passphrase, const uint8_t* note, size_t length,
          Workspace& workspace) {
  Wipe wipe{workspace.blob, sizeof(workspace.blob)};
  if (!pathIn(source, "/crossink/drawings/") || !pathIn(destination, "/crossink/stego/") || !keyValid(passphrase) ||
      !note || !length || length > kMaxNote || Storage.exists(destination))
    return fail();
  size_t encrypted = 0;
  if (securestore::encrypt(note, length, passphrase, workspace.blob, sizeof(workspace.blob), &encrypted) !=
      securestore::Status::Ok)
    return fail();
  auto input = Storage.open(source);
  if (!input) return fail();
  uint32_t end = 0;
  if (!header(input, end) || input.fileSize64() != end || !input.seekSet(0)) {
    input.close();
    return fail();
  }
  auto output = Storage.open(destination, O_WRITE | O_CREAT | O_EXCL);
  if (!output) {
    input.close();
    return fail();
  }
  uint8_t buffer[128];
  uint32_t remaining = end;
  bool ok = true;
  while (ok && remaining) {
    const size_t count = std::min<size_t>(remaining, sizeof(buffer));
    ok = input.read(buffer, count) == count && output.write(buffer, count) == count;
    remaining -= count;
  }
  const bool inputClosed = input.close();
  ok = ok && inputClosed;
  const uint32_t size = static_cast<uint32_t>(encrypted);
  uint8_t trailer[8] = {
      'S', 'C', 'V', '1', uint8_t(size), uint8_t(size >> 8), uint8_t(size >> 16), uint8_t(size >> 24)};
  if (ok)
    ok = output.write(trailer, sizeof(trailer)) == sizeof(trailer) &&
         output.write(workspace.blob, encrypted) == encrypted;
  if (ok) ok = output.sync();
  const bool closed = output.close();
  if (!ok || !closed) {
    if (!Storage.remove(destination)) LOG_ERR("StegoBmp", "Incomplete output retained; cleanup failed");
    return fail();
  }
  return true;
}
bool reveal(const char* source, const char* passphrase, uint8_t* note, size_t capacity, size_t& length,
            Workspace& workspace) {
  Wipe wipe{workspace.blob, sizeof(workspace.blob)};
  length = 0;
  if (!note || !capacity || capacity > kMaxNote) return fail();
  Wipe plaintext{note, capacity};
  if (!pathIn(source, "/crossink/stego/") || !keyValid(passphrase)) return fail();
  auto input = Storage.open(source);
  if (!input) return fail();
  uint32_t end = 0;
  if (!header(input, end) || !input.seekSet(end)) {
    input.close();
    return fail();
  }
  uint8_t trailer[8];
  bool ok = input.read(trailer, sizeof(trailer)) == sizeof(trailer) && memcmp(trailer, "SCV1", 4) == 0;
  const uint32_t size = ok ? get32(trailer + 4) : 0;
  ok = ok && size >= securestore::kHeaderBytes && size <= sizeof(workspace.blob) && input.fileSize64() == end + 8 + size;
  if (ok) ok = input.read(workspace.blob, size) == size;
  const bool closed = input.close();
  if (!ok || !closed) return fail();
  if (securestore::decrypt(workspace.blob, size, passphrase, note, capacity, &length) != securestore::Status::Ok ||
      !length) {
    length = 0;
    return fail();
  }
  plaintext.data = nullptr;
  return true;
}
}  // namespace stegobmp
