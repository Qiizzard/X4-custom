#pragma once
// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
// Cipher transforms. Deliberately free of Arduino, the HAL and the UI so the
// same code that runs on device is what the host tests exercise
// (test/cipher/). The activity is a thin shell over apply().
//
// These are classroom/puzzle ciphers, not protection (rule 21 -- a
// security-labelled feature either works and is reviewed, or it is removed;
// this one is labelled a toy instead and never claims otherwise). Nothing
// here is presented as, or should be mistaken for, lib/SecureStore.
//
// Every transform writes into a caller-supplied buffer and reports overflow
// instead of growing a std::string (rules 1 and 3).

#include <cstddef>
#include <cstdint>

namespace cipher {

enum class Algorithm : uint8_t {
  Rot13,
  Caesar,
  Vigenere,
  XorEncryptHex,
  XorDecryptHex,
  Atbash,
  Base64Encode,
  Base64Decode,
};

inline constexpr size_t kAlgorithmCount = 8;
inline constexpr Algorithm kAlgorithms[kAlgorithmCount] = {
    Algorithm::Rot13,         Algorithm::Caesar, Algorithm::Vigenere,     Algorithm::XorEncryptHex,
    Algorithm::XorDecryptHex, Algorithm::Atbash, Algorithm::Base64Encode, Algorithm::Base64Decode,
};

// Display name. Never allocates; points into flash.
const char* nameFor(Algorithm algo);

// Whether this transform reads a key/shift field at all.
bool needsKey(Algorithm algo);

// Worst-case output capacity for an input of `inputLength` bytes, for sizing a
// caller buffer once at compile time (RULESET rule 1: know the cap up front).
// Base64 encode and hex encoding both grow the input; everything else is
// same-length or shorter.
constexpr size_t outputCapacityFor(const size_t inputLength) {
  // Base64: ceil(n/3)*4 + 1. Hex: n*2 + 1. Hex is the larger of the two for
  // any n, so it sets the bound; +1 for the terminator either way.
  return inputLength * 2 + 1;
}

// Applies `algo` to null-terminated `input`, using `key` (may be empty; empty
// is only valid for algorithms where needsKey() is false, except Caesar,
// which treats a missing/non-numeric key as shift 0 -- a harmless no-op
// rather than a crash). `key` is not used for algorithms that don't need one.
//
// Writes a null-terminated result to `out`. Returns the number of characters
// written excluding the terminator, or 0 if `out` is too small or the input
// is malformed for the chosen algorithm (Base64Decode on invalid input) --
// out[0] is set to '\0' in that case rather than left holding a partial or
// wrong answer (rule 16: a truncated or partial result would look like a
// valid one).
size_t apply(Algorithm algo, const char* input, const char* key, char* out, size_t outCapacity);

}  // namespace cipher
