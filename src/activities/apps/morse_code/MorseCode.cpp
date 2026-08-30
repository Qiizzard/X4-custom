// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
#include "MorseCode.h"

#include <cstring>

namespace morse {
namespace {

// A-Z then 0-9, index-aligned with kCodes. constexpr => flash (rule 4).
constexpr char kChars[kSymbolCount] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
                                       'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                       'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

constexpr const char* kCodes[kSymbolCount] = {
    ".-",   "-...", "-.-.",  "-..",   ".",     "..-.",  "--.",   "....",  "..",    ".---",  "-.-",   ".-..",
    "--",   "-.",   "---",   ".--.",  "--.-",  ".-.",   "...",   "-",     "..-",   "...-",  ".--",   "-..-",
    "-.--", "--..", "-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----."};

char toUpperAscii(const char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; }

// Append src to out at *offset if it fits. Returns false on overflow.
//
// Both helpers keep the buffer null-terminated after every append. The capacity
// check already reserves the byte for it, so this costs one store and means the
// buffer is a valid C string at every point -- not only once the caller
// remembers to terminate it on the way out.
bool appendChecked(char* out, const size_t outCapacity, size_t* offset, const char* src) {
  const size_t length = std::strlen(src);
  if (*offset + length + 1 > outCapacity) return false;
  std::memcpy(out + *offset, src, length);
  *offset += length;
  out[*offset] = '\0';
  return true;
}

bool appendCharChecked(char* out, const size_t outCapacity, size_t* offset, const char c) {
  if (*offset + 2 > outCapacity) return false;
  out[*offset] = c;
  ++(*offset);
  out[*offset] = '\0';
  return true;
}

}  // namespace

const char* codeFor(const char character) {
  const char upper = toUpperAscii(character);
  for (size_t i = 0; i < kSymbolCount; ++i) {
    if (kChars[i] == upper) return kCodes[i];
  }
  return nullptr;
}

size_t encode(const char* text, char* out, const size_t outCapacity) {
  if (out == nullptr || outCapacity == 0) return 0;
  out[0] = '\0';
  if (text == nullptr) return 0;

  size_t offset = 0;
  bool needSeparator = false;
  for (const char* p = text; *p != '\0'; ++p) {
    if (*p == ' ') {
      if (needSeparator && !appendCharChecked(out, outCapacity, &offset, ' ')) {
        out[0] = '\0';
        return 0;
      }
      if (!appendCharChecked(out, outCapacity, &offset, '/')) {
        out[0] = '\0';
        return 0;
      }
      needSeparator = true;
      continue;
    }
    const char* code = codeFor(*p);
    if (code == nullptr) continue;  // punctuation and the like have no code here
    if (needSeparator && !appendCharChecked(out, outCapacity, &offset, ' ')) {
      out[0] = '\0';
      return 0;
    }
    if (!appendChecked(out, outCapacity, &offset, code)) {
      // Truncation would produce a *valid-looking* but wrong message, so
      // report failure instead (rule 16).
      out[0] = '\0';
      return 0;
    }
    needSeparator = true;
  }
  out[offset] = '\0';
  return offset;
}

size_t decode(const char* morseText, char* out, const size_t outCapacity) {
  if (out == nullptr || outCapacity == 0) return 0;
  out[0] = '\0';
  if (morseText == nullptr) return 0;

  size_t offset = 0;
  char token[kMaxCodeLength + 1] = {};
  size_t tokenLength = 0;
  // Set when a run of symbols is longer than any real code. Tracked separately
  // so the token buffer never has to hold the overflow.
  bool tokenTooLong = false;

  // Flush the accumulated token as one decoded character.
  const auto flush = [&]() -> bool {
    if (tokenLength == 0 && !tokenTooLong) return true;
    char decoded = '?';
    if (!tokenTooLong) {
      token[tokenLength] = '\0';
      for (size_t i = 0; i < kSymbolCount; ++i) {
        if (std::strcmp(token, kCodes[i]) == 0) {
          decoded = kChars[i];
          break;
        }
      }
    }
    tokenLength = 0;
    tokenTooLong = false;
    return appendCharChecked(out, outCapacity, &offset, decoded);
  };

  for (const char* p = morseText; *p != '\0'; ++p) {
    if (*p == '.' || *p == '-') {
      if (tokenLength >= kMaxCodeLength) {
        // Longer than any real code: remember that and let flush() emit '?'.
        tokenTooLong = true;
        continue;
      }
      token[tokenLength++] = *p;
      continue;
    }
    if (*p == '/') {
      if (!flush()) {
        out[0] = '\0';
        return 0;
      }
      if (!appendCharChecked(out, outCapacity, &offset, ' ')) {
        out[0] = '\0';
        return 0;
      }
      continue;
    }
    // Anything else (space, tab, newline) ends the current code.
    if (!flush()) {
      out[0] = '\0';
      return 0;
    }
  }
  if (!flush()) {
    out[0] = '\0';
    return 0;
  }
  out[offset] = '\0';
  return offset;
}

}  // namespace morse
