#pragma once
// SecureStore -- authenticated encryption for anything on the SD card that a
// user would mind someone reading.
//
// RULESET.md rule 22 bans the pattern this replaces: XOR keyed on the device
// MAC. That scheme fails for a reason worth stating, because it is the reason
// this file exists -- the "key" is printed on the device, broadcast in every
// frame it transmits, and readable by any app on the network. It stops a text
// editor and nothing else, which is the "security theatre" class rule 21 exists
// to keep out of this firmware.
//
// What replaces it:
//   * The key comes from a passphrase the user knows, never from device
//     identity. Nothing recoverable from the hardware decrypts the data.
//   * PBKDF2-HMAC-SHA256 stretches that passphrase against a per-blob random
//     salt, so a weak PIN costs an attacker real time per guess and two devices
//     with the same PIN produce unrelated keys.
//   * AES-256-GCM *authenticates* as well as encrypts. A wrong passphrase, a
//     flipped bit, or an edited header fails cleanly -- decrypt() returns an
//     error instead of plausible garbage.
//
// That last property is what makes a duress vault honest rather than theatre:
// the real PIN and the duress PIN are simply two passphrases over two blobs.
// Each one authenticates its own vault and fails on the other, so the decoy is
// not a UI trick that a dump of the SD card would see straight through.
//
// Threat model -- what this does and does not cover:
//   IN scope: someone who takes the SD card, or reads it over USB/WiFi transfer,
//     without knowing the passphrase. They get ciphertext plus a salt and an
//     iteration count, which is all a correct KDF is supposed to leak.
//   OUT of scope: an attacker with the unlocked device in hand while a vault is
//     open (the plaintext is in RAM by definition); anyone who learns the
//     passphrase; and physical attacks on the flash. There is no secure element
//     on a C3, so no software here can claim otherwise.
//
// This file deliberately depends on mbedtls (already in the firmware for OTA
// SHA-256) and nothing else -- no Arduino, no HAL -- so the same code path that
// runs on device is what the unit tests exercise on the host.

#include <cstddef>
#include <cstdint>

namespace securestore {

inline constexpr size_t kKeyBytes = 32;   // AES-256
inline constexpr size_t kSaltBytes = 16;  // per-blob, random
inline constexpr size_t kIvBytes = 12;    // GCM's native nonce size
inline constexpr size_t kTagBytes = 16;   // full-length GCM tag
inline constexpr size_t kHeaderBytes = 60;

// Cost of one passphrase guess. Higher is better for the user and worse for an
// attacker, bounded by what an unlock can spend on a 160 MHz single core.
// MUST be measured on hardware and tuned so an unlock lands near 500 ms; see
// docs/merge/ACCEPTANCE.md. Old blobs keep working when this changes because
// each blob records the iteration count it was written with.
inline constexpr uint32_t kDefaultIterations = 50000;

// The smallest passphrase worth encrypting under. A 4-digit PIN is only 10,000
// possibilities, so the KDF cost -- not the PIN -- is what buys the time; the
// UI should say so rather than implying a short PIN is strong.
inline constexpr size_t kMinPassphraseBytes = 4;

enum class Status : uint8_t {
  Ok,
  BadArgument,    // null pointer, undersized output buffer, empty passphrase
  OutOfMemory,    // context allocation failed -- caller should LOG_ERR + fall back
  RandomFailure,  // could not draw a salt/IV; never encrypt without one
  KdfFailure,     // PBKDF2 failed internally
  CryptoFailure,  // AES/GCM failed internally
  BadFormat,      // not a SecureStore blob, or a version we cannot read
  AuthFailed,     // wrong passphrase, or the blob was tampered with/corrupted
};

const char* statusName(Status status);

// Bytes a blob occupies for a given plaintext length. Use this to size the
// buffer handed to encrypt() -- there is no growth and no allocation inside.
constexpr size_t encryptedSize(const size_t plaintextLength) { return kHeaderBytes + plaintextLength; }

// Plaintext length a blob will yield, without decrypting it. Returns false if
// the buffer is not a well-formed blob. Cheap: reads the header only.
bool plaintextSize(const uint8_t* blob, size_t blobLength, size_t* outLength);

// Encrypt plaintext under passphrase into out (>= encryptedSize(plaintextLength)).
// Draws a fresh random salt and IV every call, so encrypting the same vault
// twice yields different bytes -- an observer cannot tell that nothing changed.
// Writes the blob length to *outLength.
Status encrypt(const uint8_t* plaintext, size_t plaintextLength, const char* passphrase, uint8_t* out,
               size_t outCapacity, size_t* outLength, uint32_t iterations = kDefaultIterations);

// Decrypt a blob into out (>= the plaintextSize() of the blob).
// Returns AuthFailed for a wrong passphrase AND for tampering -- the two are
// deliberately indistinguishable to the caller. On any failure nothing is
// written to out; there is no partial-plaintext path for a caller to mistake
// for success.
Status decrypt(const uint8_t* blob, size_t blobLength, const char* passphrase, uint8_t* out, size_t outCapacity,
               size_t* outLength);

// True if the passphrase opens the blob, without keeping the plaintext.
// For unlock screens that only need a yes/no. Costs a full KDF pass by design:
// this is the check an attacker would have to repeat.
bool verifyPassphrase(const uint8_t* blob, size_t blobLength, const char* passphrase);

// Wipe a buffer that held key material or plaintext. Routes to mbedtls's
// zeroize, which the compiler is not permitted to optimise away the way a
// plain memset on a dying buffer can be.
void secureZero(void* buffer, size_t length);

}  // namespace securestore
