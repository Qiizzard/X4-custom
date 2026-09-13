#pragma once
#include <SecureStore.h>

#include <cstddef>
#include <cstdint>

namespace stegobmp {
constexpr size_t kMaxNote = 512;
// Caller owns this outside the task stack; reused, wiped after each operation.
struct Workspace {
  uint8_t blob[securestore::encryptedSize(kMaxNote)]{};
};
// App-owned ASCII filenames only. Source: /crossink/drawings/; destination
// and reveal: /crossink/stego/. Parent directories must already exist.
// Never overwrite an existing destination. This appends encrypted data after
// the BMP's declared end; it is discoverable concealment, not pixel steganography.
// All buffers, paths, passphrase and length metadata must be disjoint.
bool hide(const char* source, const char* destination, const char* passphrase, const uint8_t* note, size_t length,
          Workspace& workspace);
// Caller wipes successful plaintext on lock/exit. Valid output buffers and
// workspace are wiped on failure. Invalid output bounds rejected untouched.
bool reveal(const char* source, const char* passphrase, uint8_t* note, size_t capacity, size_t& length,
            Workspace& workspace);
}  // namespace stegobmp
