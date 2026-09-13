#include "PasswordRecords.h"

#include <Logging.h>
#include <SecureStore.h>

#include <cstring>

namespace {
bool fits(const char* value, size_t capacity, bool required) {
  return value && (!required || value[0]) && strnlen(value, capacity) < capacity;
}
bool invalid() {
  LOG_ERR("VaultRecords", "Invalid record operation");
  return false;
}
}  // namespace
PasswordRecords::~PasswordRecords() { lock(); }
void PasswordRecords::lock() {
  securestore::secureZero(records, sizeof(records));
  count = 0;
}
bool PasswordRecords::put(size_t index, const char* title, const char* username, const char* password) {
  if (index > count || index >= kMaxRecords || !fits(title, 32, true) || !fits(username, 48, false) ||
      !fits(password, 64, true))
    return invalid();
  // 144-byte local copy supports edits using fields from the same record.
  Record replacement;
  memcpy(replacement.title, title, strlen(title) + 1);
  memcpy(replacement.username, username, strlen(username) + 1);
  memcpy(replacement.password, password, strlen(password) + 1);
  securestore::secureZero(&records[index], sizeof(Record));
  memcpy(&records[index], &replacement, sizeof(Record));
  securestore::secureZero(&replacement, sizeof(replacement));
  if (index == count) ++count;
  return true;
}
bool PasswordRecords::erase(size_t index) {
  if (index >= count) return invalid();
  for (size_t i = index; i + 1 < count; ++i) records[i] = records[i + 1];
  securestore::secureZero(&records[--count], sizeof(Record));
  return true;
}
bool PasswordRecords::encode(uint8_t* output, size_t capacity) const {
  if (!output || capacity < kEncodedBytes) return invalid();
  memset(output, 0, kEncodedBytes);
  memcpy(output, "PVR1", 4);
  output[4] = count;
  for (size_t i = 0; i < count; ++i) {
    uint8_t* row = output + 8 + i * 144;
    memcpy(row, records[i].title, 32);
    memcpy(row + 32, records[i].username, 48);
    memcpy(row + 80, records[i].password, 64);
  }
  return true;
}
bool PasswordRecords::decode(const uint8_t* input, size_t length) {
  lock();
  if (!input || length != kEncodedBytes || memcmp(input, "PVR1", 4) || input[4] > kMaxRecords || input[5] || input[6] ||
      input[7])
    return invalid();
  // Validate every field before making any plaintext record visible.
  for (size_t i = 0; i < input[4]; ++i) {
    const uint8_t* row = input + 8 + i * 144;
    if (!row[0] || !row[80] || !memchr(row, 0, 32) || !memchr(row + 32, 0, 48) || !memchr(row + 80, 0, 64))
      return invalid();
  }
  for (size_t i = 0; i < input[4]; ++i) {
    const uint8_t* row = input + 8 + i * 144;
    // Copy only meaningful string bytes; padding cannot retain hidden plaintext.
    memcpy(records[i].title, row, strlen(reinterpret_cast<const char*>(row)) + 1);
    memcpy(records[i].username, row + 32, strlen(reinterpret_cast<const char*>(row + 32)) + 1);
    memcpy(records[i].password, row + 80, strlen(reinterpret_cast<const char*>(row + 80)) + 1);
  }
  count = input[4];
  return true;
}
