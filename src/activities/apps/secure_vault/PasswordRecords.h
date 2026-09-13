#pragma once
#include <cstddef>
#include <cstdint>

// Plaintext working set only. Own this in the vault activity, never globally.
// Copying is forbidden; lock()/destruction wipe the full fixed record storage.
class PasswordRecords {
 public:
  static constexpr size_t kMaxRecords = 8;
  static constexpr size_t kEncodedBytes = 8 + kMaxRecords * 144;
  struct Record {
    char title[32]{};
    char username[48]{};
    char password[64]{};
  };
  ~PasswordRecords();
  PasswordRecords() = default;
  PasswordRecords(const PasswordRecords&) = delete;
  PasswordRecords& operator=(const PasswordRecords&) = delete;
  size_t size() const { return count; }
  const Record* at(size_t index) const { return index < count ? &records[index] : nullptr; }
  bool put(size_t index, const char* title, const char* username, const char* password);
  bool erase(size_t index);
  void lock();
  // Encoding buffer must be disjoint from this object and wiped by the caller.
  bool encode(uint8_t* output, size_t capacity) const;
  // Input must be disjoint from this object; invalid input clears all records.
  bool decode(const uint8_t* input, size_t length);

 private:
  Record records[kMaxRecords]{};
  size_t count = 0;
};
