// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
#include "Cipher.h"

#include <cstring>

namespace cipher {
namespace {

constexpr char kHexDigits[] = "0123456789ABCDEF";
constexpr char kB64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

bool isAsciiAlpha(const char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

char toUpperAscii(const char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; }

int hexValue(const char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

int b64Value(const char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

// Same-length substitution: shifts every letter by `shift`, leaves everything
// else untouched. Used by rot13, caesar and atbash (shift derived per-call).
size_t shiftSubstitute(const char* input, char* out, const size_t outCapacity, int (*shiftFor)(char, void* ctx),
                       void* ctx) {
  const size_t length = std::strlen(input);
  if (length + 1 > outCapacity) return 0;
  for (size_t i = 0; i < length; ++i) {
    const char c = input[i];
    if (c >= 'a' && c <= 'z') {
      const int shift = shiftFor(c, ctx);
      out[i] = static_cast<char>('a' + (((c - 'a' + shift) % 26 + 26) % 26));
    } else if (c >= 'A' && c <= 'Z') {
      const int shift = shiftFor(c, ctx);
      out[i] = static_cast<char>('A' + (((c - 'A' + shift) % 26 + 26) % 26));
    } else {
      out[i] = c;
    }
  }
  out[length] = '\0';
  return length;
}

size_t applyRot13(const char* input, char* out, const size_t outCapacity) {
  auto shiftFor = [](char, void*) -> int { return 13; };
  return shiftSubstitute(input, out, outCapacity, shiftFor, nullptr);
}

size_t applyCaesar(const char* input, const char* key, char* out, const size_t outCapacity) {
  // Preserve decimal-prefix/sign parsing, but reduce each digit modulo 26
  // so even a maximum-length keyboard value cannot overflow integer math.
  int shift = 0;
  if (key != nullptr) {
    while (*key == ' ' || (*key >= '\t' && *key <= '\r')) ++key;
    const bool negative = *key == '-';
    if (*key == '-' || *key == '+') ++key;
    while (*key >= '0' && *key <= '9') {
      shift = (shift * 10 + (*key++ - '0')) % 26;
    }
    if (negative) shift = -shift;
  }
  struct Ctx {
    int shift;
  } ctx{shift};
  auto shiftFor = [](char, void* c) -> int { return static_cast<Ctx*>(c)->shift; };
  return shiftSubstitute(input, out, outCapacity, shiftFor, &ctx);
}

size_t applyAtbash(const char* input, char* out, const size_t outCapacity) {
  const size_t length = std::strlen(input);
  if (length + 1 > outCapacity) return 0;
  for (size_t i = 0; i < length; ++i) {
    const char c = input[i];
    if (c >= 'a' && c <= 'z') {
      out[i] = static_cast<char>('z' - (c - 'a'));
    } else if (c >= 'A' && c <= 'Z') {
      out[i] = static_cast<char>('Z' - (c - 'A'));
    } else {
      out[i] = c;
    }
  }
  out[length] = '\0';
  return length;
}

// Vigenere needs an alphabetic key. A key with no letters at all (empty, all
// digits/punctuation) is malformed input for this algorithm -- report failure
// rather than silently no-op'ing or indexing garbage (rule 16).
size_t applyVigenere(const char* input, const char* key, char* out, const size_t outCapacity) {
  if (key == nullptr) return 0;
  char sanitizedKey[33];
  size_t keyLength = 0;
  for (const char* k = key; *k != '\0' && keyLength < sizeof(sanitizedKey) - 1; ++k) {
    if (isAsciiAlpha(*k)) sanitizedKey[keyLength++] = static_cast<char>(toUpperAscii(*k) - 'A');
  }
  if (keyLength == 0) return 0;

  const size_t length = std::strlen(input);
  if (length + 1 > outCapacity) return 0;
  size_t keyIndex = 0;
  for (size_t i = 0; i < length; ++i) {
    const char c = input[i];
    if (c >= 'a' && c <= 'z') {
      out[i] = static_cast<char>('a' + (c - 'a' + sanitizedKey[keyIndex % keyLength]) % 26);
      ++keyIndex;
    } else if (c >= 'A' && c <= 'Z') {
      out[i] = static_cast<char>('A' + (c - 'A' + sanitizedKey[keyIndex % keyLength]) % 26);
      ++keyIndex;
    } else {
      out[i] = c;
    }
  }
  out[length] = '\0';
  return length;
}

// XOR is self-inverse on raw bytes, but the ciphertext bytes are not
// generally printable ASCII -- drawing them straight to the e-ink display as
// "text" is how a toy cipher app corrupts its own UI. Encrypt always hex-
// encodes its output; decrypt always expects hex input. That keeps every
// encoded ciphertext printable. Decoded bytes may still be binary or use an
// incorrect key; callers must not infer authentication or printable output.
size_t applyXorEncryptHex(const char* input, const char* key, char* out, const size_t outCapacity) {
  if (key == nullptr || key[0] == '\0') return 0;
  const size_t inputLength = std::strlen(input);
  const size_t keyLength = std::strlen(key);
  if (inputLength * 2 + 1 > outCapacity) return 0;
  for (size_t i = 0; i < inputLength; ++i) {
    const uint8_t b = static_cast<uint8_t>(input[i]) ^ static_cast<uint8_t>(key[i % keyLength]);
    out[i * 2] = kHexDigits[(b >> 4) & 0x0F];
    out[i * 2 + 1] = kHexDigits[b & 0x0F];
  }
  out[inputLength * 2] = '\0';
  return inputLength * 2;
}

size_t applyXorDecryptHex(const char* input, const char* key, char* out, const size_t outCapacity) {
  if (key == nullptr || key[0] == '\0') return 0;
  const size_t inputLength = std::strlen(input);
  if (inputLength % 2 != 0) return 0;  // not valid hex pairs
  const size_t outLength = inputLength / 2;
  if (outLength + 1 > outCapacity) return 0;
  const size_t keyLength = std::strlen(key);
  for (size_t i = 0; i < outLength; ++i) {
    const int hi = hexValue(input[i * 2]);
    const int lo = hexValue(input[i * 2 + 1]);
    if (hi < 0 || lo < 0) {
      out[0] = '\0';  // Never expose a partially decoded result on failure.
      return 0;
    }
    const auto byte = static_cast<uint8_t>((hi << 4) | lo);
    out[i] = static_cast<char>(byte ^ static_cast<uint8_t>(key[i % keyLength]));
  }
  out[outLength] = '\0';
  return outLength;
}

size_t applyBase64Encode(const char* input, char* out, const size_t outCapacity) {
  const auto* data = reinterpret_cast<const uint8_t*>(input);
  const size_t length = std::strlen(input);
  const size_t needed = ((length + 2) / 3) * 4 + 1;
  if (needed > outCapacity) return 0;
  size_t o = 0;
  for (size_t i = 0; i < length; i += 3) {
    const uint32_t v = (static_cast<uint32_t>(data[i]) << 16) |
                       (i + 1 < length ? static_cast<uint32_t>(data[i + 1]) << 8 : 0) |
                       (i + 2 < length ? static_cast<uint32_t>(data[i + 2]) : 0);
    out[o++] = kB64Alphabet[(v >> 18) & 0x3F];
    out[o++] = kB64Alphabet[(v >> 12) & 0x3F];
    out[o++] = (i + 1 < length) ? kB64Alphabet[(v >> 6) & 0x3F] : '=';
    out[o++] = (i + 2 < length) ? kB64Alphabet[v & 0x3F] : '=';
  }
  out[o] = '\0';
  return o;
}

// Strict: any character outside the base64 alphabet (other than padding at
// the very end) is malformed input, not something to skip past. Silently
// skipping bad characters -- what this app's biscuit ancestor did -- turns a
// mistyped ciphertext into a *different, plausible-looking* plaintext instead
// of a visible error (rule 16).
size_t applyBase64Decode(const char* input, char* out, const size_t outCapacity) {
  const size_t inputLength = std::strlen(input);
  if (inputLength == 0 || inputLength % 4 != 0) return 0;
  size_t significant = inputLength;
  while (significant > 0 && input[significant - 1] == '=') --significant;
  if (inputLength - significant > 2) return 0;  // at most 2 padding chars

  const size_t maxOut = (inputLength / 4) * 3 + 1;
  if (maxOut > outCapacity) return 0;

  size_t o = 0;
  uint32_t buffer = 0;
  int bits = 0;
  for (size_t i = 0; i < significant; ++i) {
    const int v = b64Value(input[i]);
    if (v < 0) {
      out[0] = '\0';
      return 0;
    }
    buffer = (buffer << 6) | static_cast<uint32_t>(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out[o++] = static_cast<char>((buffer >> bits) & 0xFF);
    }
  }
  out[o] = '\0';
  return o;
}

}  // namespace

const char* nameFor(const Algorithm algo) {
  switch (algo) {
    case Algorithm::Rot13:
      return "ROT13";
    case Algorithm::Caesar:
      return "Caesar";
    case Algorithm::Vigenere:
      return "Vigenere";
    case Algorithm::XorEncryptHex:
      return "XOR Encrypt (hex)";
    case Algorithm::XorDecryptHex:
      return "XOR Decrypt (hex)";
    case Algorithm::Atbash:
      return "Atbash";
    case Algorithm::Base64Encode:
      return "Base64 Encode";
    case Algorithm::Base64Decode:
      return "Base64 Decode";
  }
  return "";
}

bool needsKey(const Algorithm algo) {
  return algo == Algorithm::Caesar || algo == Algorithm::Vigenere || algo == Algorithm::XorEncryptHex ||
         algo == Algorithm::XorDecryptHex;
}

size_t apply(const Algorithm algo, const char* input, const char* key, char* out, const size_t outCapacity) {
  if (out == nullptr || outCapacity == 0) return 0;
  out[0] = '\0';
  if (input == nullptr) return 0;

  switch (algo) {
    case Algorithm::Rot13:
      return applyRot13(input, out, outCapacity);
    case Algorithm::Caesar:
      return applyCaesar(input, key, out, outCapacity);
    case Algorithm::Vigenere:
      return applyVigenere(input, key, out, outCapacity);
    case Algorithm::XorEncryptHex:
      return applyXorEncryptHex(input, key, out, outCapacity);
    case Algorithm::XorDecryptHex:
      return applyXorDecryptHex(input, key, out, outCapacity);
    case Algorithm::Atbash:
      return applyAtbash(input, out, outCapacity);
    case Algorithm::Base64Encode:
      return applyBase64Encode(input, out, outCapacity);
    case Algorithm::Base64Decode:
      return applyBase64Decode(input, out, outCapacity);
  }
  return 0;
}

}  // namespace cipher
