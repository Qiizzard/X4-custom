#include "EncryptedVaultFile.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>
#include <limits>

namespace vaultfile {
namespace {
// Caller supplies activity-owned buffers; no large local arrays or allocations.
struct Wipe {
  uint8_t* pointer;
  size_t size;
  ~Wipe() {
    if (pointer) securestore::secureZero(pointer, size);
  }
};
bool overlaps(const void* a, size_t aSize, const void* b, size_t bSize) {
  const auto left = reinterpret_cast<uintptr_t>(a), right = reinterpret_cast<uintptr_t>(b);
  if (aSize > std::numeric_limits<uintptr_t>::max() - left || bSize > std::numeric_limits<uintptr_t>::max() - right)
    return true;
  return left < right + bSize && right < left + aSize;
}
bool validPath(const char* path) {
  // App-owned single filename under the vault directory; no traversal.
  static constexpr char prefix[] = "/crossink/vaults/";
  if (!path || strncmp(path, prefix, sizeof(prefix) - 1)) return false;
  const char* name = path + sizeof(prefix) - 1;
  const size_t size = strnlen(name, 65);
  if (!size || size > 64 || name[0] == '.') return false;
  for (size_t i = 0; i < size; ++i) {
    const char c = name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
          c == '.'))
      return false;
  }
  return true;
}
Result failed(Status status) {
  LOG_ERR("VaultFile", "Operation failed (%d)", static_cast<int>(status));
  return {status};
}
}  // namespace

Result load(const char* path, const char* passphrase, uint8_t* plaintext, size_t capacity, size_t* length,
            uint8_t* scratch, size_t scratchCapacity) {
  // Reject impossible capacities without trusting them for a wipe.
  if (!plaintext || !scratch || !length || !capacity || capacity > kMaxPlaintext ||
      scratchCapacity < securestore::kHeaderBytes || scratchCapacity > kMaxBlob)
    return failed(Status::BadArgument);
  *length = 0;
  if (overlaps(plaintext, capacity, scratch, scratchCapacity)) {
    securestore::secureZero(plaintext, capacity);
    securestore::secureZero(scratch, scratchCapacity);
    return failed(Status::BadArgument);
  }
  Wipe scratchWipe{scratch, scratchCapacity};
  Wipe outputWipe{plaintext, capacity};
  if (!validPath(path) || !passphrase || strnlen(passphrase, 129) > 128 ||
      strlen(passphrase) < securestore::kMinPassphraseBytes)
    return failed(Status::BadArgument);
  auto file = Storage.open(path);
  if (!file) return failed(Status::IoError);
  const size_t size = file.fileSize();
  if (size < securestore::kHeaderBytes || size > scratchCapacity) {
    file.close();
    return failed(Status::TooLarge);
  }
  const bool read = file.read(scratch, size) == size;
  const bool closed = file.close();
  if (!read || !closed) return failed(Status::IoError);
  size_t expected = 0;
  if (!securestore::plaintextSize(scratch, size, &expected)) return failed(Status::InvalidBlob);
  if (expected > capacity) return failed(Status::TooLarge);
  const auto crypto = securestore::decrypt(scratch, size, passphrase, plaintext, capacity, length);
  if (crypto != securestore::Status::Ok) {
    *length = 0;
    LOG_ERR("VaultFile", "Decrypt failed (%s)", securestore::statusName(crypto));
    return {Status::CryptoError, crypto};
  }
  if (*length != expected) {
    *length = 0;
    return failed(Status::InvalidBlob);
  }
  outputWipe.pointer = nullptr;  // ownership of successful plaintext stays with caller
  return {Status::Ok};
}

Result create(const char* path, const char* passphrase, const uint8_t* plaintext, size_t length, uint8_t* scratch,
              size_t scratchCapacity) {
  if (!scratch || scratchCapacity > kMaxBlob || scratchCapacity < securestore::kHeaderBytes || !plaintext ||
      length > kMaxPlaintext)
    return failed(Status::BadArgument);
  if (overlaps(plaintext, length, scratch, scratchCapacity)) return failed(Status::BadArgument);
  Wipe scratchWipe{scratch, scratchCapacity};
  if (!validPath(path) || !passphrase || strnlen(passphrase, 129) > 128 ||
      strlen(passphrase) < securestore::kMinPassphraseBytes || securestore::encryptedSize(length) > scratchCapacity)
    return failed(Status::BadArgument);
  if (Storage.exists(path)) return failed(Status::AlreadyExists);
  size_t written = 0;
  const auto crypto = securestore::encrypt(plaintext, length, passphrase, scratch, scratchCapacity, &written);
  if (crypto != securestore::Status::Ok) {
    LOG_ERR("VaultFile", "Encrypt failed (%s)", securestore::statusName(crypto));
    return {Status::CryptoError, crypto};
  }
  // Encryption must succeed before touching disk. Exclusive creation closes
  // the race between the exists check and opening the file.
  auto file = Storage.open(path, O_WRITE | O_CREAT | O_EXCL);
  if (!file) return failed(Status::IoError);
  bool ok = file.write(scratch, written) == written;
  if (ok) ok = file.sync();
  const bool closed = file.close();
  if (!ok || !closed) {
    if (!Storage.remove(path)) LOG_ERR("VaultFile", "Could not remove incomplete new vault");
    return failed(Status::IoError);
  }
  return {Status::Ok};
}
}  // namespace vaultfile
