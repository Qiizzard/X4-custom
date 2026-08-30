#include "SecureStore.h"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/platform_util.h>

#include <cstring>
#include <memory>
#include <new>

namespace securestore {
namespace {

// Blob layout, little-endian. The whole header is fed to GCM as additional
// authenticated data, so the salt, the iteration count and the length are
// covered by the tag: an attacker cannot cut the KDF cost to 1 iteration or
// lie about the payload size without the tag check failing.
//
//   0   4   magic 'X','S','S','1'
//   4   1   version
//   5   1   kdf id (1 = PBKDF2-HMAC-SHA256)
//   6   2   reserved, must be zero
//   8   4   iterations
//  12  16   salt
//  28  12   iv
//  40  16   tag
//  56   4   plaintext length
//  60  ..   ciphertext
constexpr uint8_t kMagic[4] = {'X', 'S', 'S', '1'};
constexpr uint8_t kVersion = 1;
constexpr uint8_t kKdfPbkdf2HmacSha256 = 1;

constexpr size_t kOffMagic = 0;
constexpr size_t kOffVersion = 4;
constexpr size_t kOffKdf = 5;
constexpr size_t kOffReserved = 6;
constexpr size_t kOffIterations = 8;
constexpr size_t kOffSalt = 12;
constexpr size_t kOffIv = 28;
constexpr size_t kOffTag = 40;
constexpr size_t kOffLength = 56;

// An upper bound on iterations keeps a corrupted or hostile header from
// freezing the UI for minutes on a 160 MHz core before the tag check can even
// run. Chosen well above any value we would ship.
constexpr uint32_t kMaxIterations = 2000000;

void writeU32(uint8_t* p, const uint32_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFF);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
  p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
  p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

uint32_t readU32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

// RAII around the mbedtls contexts. These are heap-allocated rather than stack
// locals on purpose: a gcm context plus a drbg context is well past the 256-byte
// stack budget RULESET.md rule 15 sets, and this can be called from an activity
// task with a 4 KB stack. Allocation happens once per call, never in a loop.
struct GcmContext {
  mbedtls_gcm_context ctx;
  GcmContext() { mbedtls_gcm_init(&ctx); }
  ~GcmContext() { mbedtls_gcm_free(&ctx); }
  GcmContext(const GcmContext&) = delete;
  GcmContext& operator=(const GcmContext&) = delete;
};

struct RandomContext {
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context drbg;
  RandomContext() {
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&drbg);
  }
  ~RandomContext() {
    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_entropy_free(&entropy);
  }
  RandomContext(const RandomContext&) = delete;
  RandomContext& operator=(const RandomContext&) = delete;
};

// Fill buffer with cryptographic randomness. Seeded from mbedtls's entropy
// pool, which on ESP-IDF is backed by the hardware RNG. A failure here must
// abort the encrypt: a predictable IV would break GCM outright, so there is no
// "fall back to something weaker" path.
bool drawRandom(uint8_t* buffer, const size_t length) {
  auto rng = std::unique_ptr<RandomContext>(new (std::nothrow) RandomContext());
  if (!rng) return false;
  static constexpr unsigned char kPers[] = "crossink-securestore";
  if (mbedtls_ctr_drbg_seed(&rng->drbg, mbedtls_entropy_func, &rng->entropy, kPers, sizeof(kPers) - 1) != 0) {
    return false;
  }
  return mbedtls_ctr_drbg_random(&rng->drbg, buffer, length) == 0;
}

// PBKDF2-HMAC-SHA256. Writes exactly kKeyBytes into key.
bool deriveKey(const char* passphrase, const size_t passphraseLength, const uint8_t* salt, const uint32_t iterations,
               uint8_t* key) {
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (info == nullptr) return false;
  return mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256, reinterpret_cast<const unsigned char*>(passphrase),
                                       passphraseLength, salt, kSaltBytes, iterations, kKeyBytes, key) == 0;
}

// Validate a candidate blob's header. Returns Ok and fills the fields the
// caller needs, or the specific reason it is unusable.
Status parseHeader(const uint8_t* blob, const size_t blobLength, uint32_t* iterations, size_t* plaintextLength) {
  if (blob == nullptr || blobLength < kHeaderBytes) return Status::BadFormat;
  if (std::memcmp(blob + kOffMagic, kMagic, sizeof(kMagic)) != 0) return Status::BadFormat;
  if (blob[kOffVersion] != kVersion) return Status::BadFormat;
  if (blob[kOffKdf] != kKdfPbkdf2HmacSha256) return Status::BadFormat;
  if (blob[kOffReserved] != 0 || blob[kOffReserved + 1] != 0) return Status::BadFormat;

  const uint32_t iters = readU32(blob + kOffIterations);
  if (iters == 0 || iters > kMaxIterations) return Status::BadFormat;

  const uint32_t length = readU32(blob + kOffLength);
  // The recorded length must match the buffer exactly. Believing a length that
  // overruns the buffer is how a header field becomes a read past the end.
  if (blobLength - kHeaderBytes != length) return Status::BadFormat;

  if (iterations != nullptr) *iterations = iters;
  if (plaintextLength != nullptr) *plaintextLength = length;
  return Status::Ok;
}

}  // namespace

const char* statusName(const Status status) {
  switch (status) {
    case Status::Ok:
      return "ok";
    case Status::BadArgument:
      return "bad argument";
    case Status::OutOfMemory:
      return "out of memory";
    case Status::RandomFailure:
      return "rng failure";
    case Status::KdfFailure:
      return "kdf failure";
    case Status::CryptoFailure:
      return "crypto failure";
    case Status::BadFormat:
      return "bad format";
    case Status::AuthFailed:
      return "auth failed";
  }
  return "unknown";
}

bool plaintextSize(const uint8_t* blob, const size_t blobLength, size_t* outLength) {
  return parseHeader(blob, blobLength, nullptr, outLength) == Status::Ok;
}

Status encrypt(const uint8_t* plaintext, const size_t plaintextLength, const char* passphrase, uint8_t* out,
               const size_t outCapacity, size_t* outLength, const uint32_t iterations) {
  if (out == nullptr || outLength == nullptr || passphrase == nullptr) return Status::BadArgument;
  if (plaintext == nullptr && plaintextLength != 0) return Status::BadArgument;
  if (iterations == 0 || iterations > kMaxIterations) return Status::BadArgument;
  const size_t passphraseLength = std::strlen(passphrase);
  if (passphraseLength < kMinPassphraseBytes) return Status::BadArgument;
  if (plaintextLength > UINT32_MAX - kHeaderBytes) return Status::BadArgument;
  const size_t blobLength = encryptedSize(plaintextLength);
  if (outCapacity < blobLength) return Status::BadArgument;

  // Header first, because it is the GCM additional-authenticated-data.
  std::memcpy(out + kOffMagic, kMagic, sizeof(kMagic));
  out[kOffVersion] = kVersion;
  out[kOffKdf] = kKdfPbkdf2HmacSha256;
  out[kOffReserved] = 0;
  out[kOffReserved + 1] = 0;
  writeU32(out + kOffIterations, iterations);
  writeU32(out + kOffLength, static_cast<uint32_t>(plaintextLength));

  // A fresh salt and IV per call. Reusing an IV under one key is the single
  // worst thing you can do to GCM, so these are drawn together and any failure
  // aborts rather than degrades.
  if (!drawRandom(out + kOffSalt, kSaltBytes) || !drawRandom(out + kOffIv, kIvBytes)) {
    return Status::RandomFailure;
  }
  // The tag is not known yet; keep it zeroed so the AAD we authenticate below
  // is byte-identical to the one decrypt() will reconstruct.
  std::memset(out + kOffTag, 0, kTagBytes);

  uint8_t key[kKeyBytes];
  if (!deriveKey(passphrase, passphraseLength, out + kOffSalt, iterations, key)) {
    secureZero(key, sizeof(key));
    return Status::KdfFailure;
  }

  auto gcm = std::unique_ptr<GcmContext>(new (std::nothrow) GcmContext());
  if (!gcm) {
    secureZero(key, sizeof(key));
    return Status::OutOfMemory;
  }

  Status status = Status::Ok;
  if (mbedtls_gcm_setkey(&gcm->ctx, MBEDTLS_CIPHER_ID_AES, key, kKeyBytes * 8) != 0) {
    status = Status::CryptoFailure;
  } else if (mbedtls_gcm_crypt_and_tag(&gcm->ctx, MBEDTLS_GCM_ENCRYPT, plaintextLength, out + kOffIv, kIvBytes, out,
                                       kHeaderBytes, plaintext, out + kHeaderBytes, kTagBytes, out + kOffTag) != 0) {
    status = Status::CryptoFailure;
  }

  secureZero(key, sizeof(key));
  if (status != Status::Ok) {
    // Leave nothing decryptable-looking behind on failure.
    secureZero(out, blobLength);
    return status;
  }

  *outLength = blobLength;
  return Status::Ok;
}

Status decrypt(const uint8_t* blob, const size_t blobLength, const char* passphrase, uint8_t* out,
               const size_t outCapacity, size_t* outLength) {
  if (passphrase == nullptr || outLength == nullptr) return Status::BadArgument;
  const size_t passphraseLength = std::strlen(passphrase);
  if (passphraseLength < kMinPassphraseBytes) return Status::BadArgument;

  uint32_t iterations = 0;
  size_t plaintextLength = 0;
  const Status headerStatus = parseHeader(blob, blobLength, &iterations, &plaintextLength);
  if (headerStatus != Status::Ok) return headerStatus;
  if (out == nullptr && plaintextLength != 0) return Status::BadArgument;
  if (outCapacity < plaintextLength) return Status::BadArgument;

  uint8_t key[kKeyBytes];
  if (!deriveKey(passphrase, passphraseLength, blob + kOffSalt, iterations, key)) {
    secureZero(key, sizeof(key));
    return Status::KdfFailure;
  }

  auto gcm = std::unique_ptr<GcmContext>(new (std::nothrow) GcmContext());
  if (!gcm) {
    secureZero(key, sizeof(key));
    return Status::OutOfMemory;
  }

  // Rebuild the AAD exactly as encrypt() authenticated it: the header with the
  // tag field zeroed. Any edit to salt, iterations or length changes this and
  // the tag check below fails.
  uint8_t aad[kHeaderBytes];
  std::memcpy(aad, blob, kHeaderBytes);
  std::memset(aad + kOffTag, 0, kTagBytes);

  Status status = Status::Ok;
  if (mbedtls_gcm_setkey(&gcm->ctx, MBEDTLS_CIPHER_ID_AES, key, kKeyBytes * 8) != 0) {
    status = Status::CryptoFailure;
  } else if (mbedtls_gcm_auth_decrypt(&gcm->ctx, plaintextLength, blob + kOffIv, kIvBytes, aad, kHeaderBytes,
                                      blob + kOffTag, kTagBytes, blob + kHeaderBytes, out) != 0) {
    // mbedtls does the tag comparison in constant time. A wrong passphrase and
    // a tampered blob land here identically, and that is intentional.
    status = Status::AuthFailed;
  }

  secureZero(key, sizeof(key));
  if (status != Status::Ok) {
    // On auth failure mbedtls may have written speculative plaintext. Wipe it:
    // unauthenticated plaintext must never reach a caller.
    if (out != nullptr && plaintextLength != 0) secureZero(out, plaintextLength);
    return status;
  }

  *outLength = plaintextLength;
  return Status::Ok;
}

bool verifyPassphrase(const uint8_t* blob, const size_t blobLength, const char* passphrase) {
  size_t plaintextLength = 0;
  if (!plaintextSize(blob, blobLength, &plaintextLength)) return false;

  // The plaintext is decrypted and immediately wiped -- GCM cannot verify a tag
  // without producing it, and there is no cheaper honest check.
  // An empty vault still needs a one-byte allocation to hand decrypt() a valid
  // pointer; the tag check is what is being tested, not the payload.
  const size_t scratchLength = plaintextLength != 0 ? plaintextLength : 1;
  auto scratch = std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[scratchLength]);
  if (!scratch) return false;

  size_t outLength = 0;
  const Status status = decrypt(blob, blobLength, passphrase, scratch.get(), scratchLength, &outLength);
  secureZero(scratch.get(), scratchLength);
  return status == Status::Ok;
}

void secureZero(void* buffer, const size_t length) {
  if (buffer != nullptr && length != 0) mbedtls_platform_zeroize(buffer, length);
}

}  // namespace securestore
