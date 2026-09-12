#pragma once
#include <SecureStore.h>

#include <cstddef>
#include <cstdint>

namespace vaultfile {
// Files contain a normal SecureStore blob, with no plaintext payload on disk.
// Caller owns buffers and must wipe successful plaintext when locking/exiting.
// Passphrase is used only during the call and is never retained here.
inline constexpr size_t kMaxPlaintext = 4096;
inline constexpr size_t kMaxBlob = securestore::encryptedSize(kMaxPlaintext);
enum class Status { Ok, BadArgument, IoError, TooLarge, InvalidBlob, CryptoError, AlreadyExists };
struct Result {
  Status status;
  securestore::Status crypto = securestore::Status::Ok;
};
// Buffers must be disjoint and must not alias path, passphrase or length.
// With valid buffer arguments, output length is zero and plaintext is wiped
// on failure. Capacities must be <= the above bounds. Valid disjoint scratch
// is wiped after operations; invalid buffer arguments are rejected untouched.
Result load(const char* path, const char* passphrase, uint8_t* plaintext, size_t capacity, size_t* length,
            uint8_t* scratch, size_t scratchCapacity);
// Never overwrites an existing path. Parent directory must already exist.
// Writes, sync and close are checked. Failure may leave an incomplete file
// if cleanup also fails; load rejects malformed/authentication-failing blobs.
// Caller must treat IoError as uncertain, not retry under an overwrite policy.
Result create(const char* path, const char* passphrase, const uint8_t* plaintext, size_t length, uint8_t* scratch,
              size_t scratchCapacity);
}  // namespace vaultfile
