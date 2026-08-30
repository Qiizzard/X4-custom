#pragma once
// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
// Morse encode/decode. Deliberately free of Arduino, the HAL and the UI so the
// same code that runs on device is what the host tests exercise
// (test/morse_code/). The activity is a thin shell over these two functions.
//
// Everything writes into a caller-supplied buffer and reports truncation
// instead of growing a std::string (RULESET rules 1 and 3). biscuit's original
// built its output with repeated `std::string +=`, which reallocates as it goes.

#include <cstddef>

namespace morse {

// International Morse: A-Z and 0-9. Longest code is 5 symbols.
inline constexpr size_t kMaxCodeLength = 5;
inline constexpr size_t kSymbolCount = 36;

// Bytes needed to encode `textLength` characters in the worst case: every
// character a 5-symbol code plus its separating space.
constexpr size_t encodedCapacityFor(const size_t textLength) { return textLength * (kMaxCodeLength + 1) + 1; }

// Encode text to Morse. Letters are upper/lower-case insensitive; a space
// becomes "/" (the standard word separator); characters with no code are
// skipped. Codes are separated by a single space.
//
// Writes a null-terminated string to `out`. Returns the number of characters
// written excluding the terminator, or 0 if `out` is too small (in which case
// out[0] is set to '\0' rather than left as a partial encoding).
size_t encode(const char* text, char* out, size_t outCapacity);

// Decode a Morse string back to text. Accepts codes separated by spaces and
// "/" as a word break. An unrecognised code becomes '?' so a decode never
// silently drops input.
//
// Writes a null-terminated string to `out`. Returns characters written
// excluding the terminator, or 0 if `out` is too small.
size_t decode(const char* morseText, char* out, size_t outCapacity);

// The code for one character, or nullptr if it has none. Never allocates.
const char* codeFor(char character);

}  // namespace morse
