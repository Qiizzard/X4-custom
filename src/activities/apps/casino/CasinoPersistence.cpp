#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

#include "CasinoActivity.h"

namespace {
constexpr size_t recordSize = 23;
const char* slotPath(int slot) { return slot == 0 ? "/crossink/casino-a.dat" : "/crossink/casino-b.dat"; }
uint32_t read32(const uint8_t* bytes) {
  uint32_t value = 0;
  for (unsigned i = 0; i < 4; ++i) value |= uint32_t(bytes[i]) << (8 * i);
  return value;
}
void write32(uint8_t* bytes, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) bytes[i] = uint8_t(value >> (8 * i));
}
uint32_t checksum(const uint8_t* bytes) {
  uint32_t value = 2166136261u;
  for (unsigned i = 0; i < 19; ++i) value = (value ^ bytes[i]) * 16777619u;
  return value;
}
}  // namespace

void CasinoActivity::loadProgress() {
  if (!Storage.ready()) {
    LOG_ERR("CASINO", "SD unavailable; progress save disabled until re-entry");
    saveError = saveBlocked = true;
    return;
  }
  bool any = false;
  for (int slot = 0; slot < 2; ++slot) {
    if (!Storage.exists(slotPath(slot))) continue;
    any = true;
    auto file = Storage.open(slotPath(slot));
    if (!file) {
      LOG_ERR("CASINO", "Cannot open save slot %d", slot);
      saveError = true;
      continue;
    }
    uint8_t bytes[recordSize] = {};
    const bool read = file.fileSize() == sizeof(bytes) && file.read(bytes, sizeof(bytes)) == sizeof(bytes);
    const bool closed = file.close();
    if (!read || !closed || memcmp(bytes, "CAS1", 4) || !read32(bytes + 4) || read32(bytes + 8) > cap ||
        (bytes[18] & 0xfc) || read32(bytes + 19) != checksum(bytes)) {
      LOG_ERR("CASINO", "Invalid save slot %d; preserving file", slot);
      saveError = true;
      continue;
    }
    if (saveSlot < 0 || read32(bytes + 4) > saveGeneration) {
      saveSlot = slot;
      saveGeneration = read32(bytes + 4);
      credits = read32(bytes + 8);
      memcpy(collected, bytes + 12, sizeof(collected));
      dirty = false;
    }
  }
  // Never overwrite unrecognized/corrupt files when there is no recoverable slot.
  saveBlocked = any && saveSlot < 0;
}

void CasinoActivity::saveProgress() {
  if (saveBlocked || saveGeneration == UINT32_MAX || !Storage.ready()) {
    LOG_ERR("CASINO", "Save unavailable, invalid slots or exhausted generation");
    saveError = true;
    return;
  }
  if (!dirty) return;
  if (!Storage.ensureDirectoryExists("/crossink")) {
    LOG_ERR("CASINO", "Cannot create save directory");
    saveError = true;
    return;
  }
  uint8_t bytes[recordSize] = {};
  memcpy(bytes, "CAS1", 4);
  write32(bytes + 4, saveGeneration + 1);
  write32(bytes + 8, credits);
  memcpy(bytes + 12, collected, sizeof(collected));
  write32(bytes + 19, checksum(bytes));
  const int target = saveSlot == 0 ? 1 : 0;
  auto file = Storage.open(slotPath(target), O_WRITE | O_CREAT | O_TRUNC);
  if (!file) {
    LOG_ERR("CASINO", "Cannot open save destination");
    saveError = true;
    return;
  }
  bool ok = file.write(bytes, sizeof(bytes)) == sizeof(bytes);
  if (ok) ok = file.sync();
  const bool closed = file.close();
  if (!ok || !closed) {
    LOG_ERR("CASINO", "Save write/sync/close failed; previous slot retained");
    saveError = true;
    return;
  }
  saveSlot = target;
  ++saveGeneration;
  dirty = false;
  saveError = false;
}
