#pragma once
// Adapted from biscuit, MIT, Copyright (c) 2025 Dave Allie.
#include <cstdint>
#include <memory>

#include "activities/Activity.h"

class EventLoggerActivity final : public Activity {
 public:
  explicit EventLoggerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("EventLogger", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct Entry {
    uint32_t uptime;
    char text[128];
  };
  static constexpr int MAX_ENTRIES = 50;
  static constexpr size_t MAX_FILE = 64 * 1024;
  static constexpr const char* LOG_PATH = "/crossink/logs/events.csv";
  // 6600 bytes, allocated only for this activity, too large for task stack.
  std::unique_ptr<Entry[]> entries;
  int count = 0, next = 0, selected = 0;
  bool viewing = false, composing = false, storageError = false;
  const Entry& entry(int i) const { return entries[(next - 1 - i + MAX_ENTRIES) % MAX_ENTRIES]; }
  void loadEntries();
  void saveEntry(const char* text);
  void compose();
};
