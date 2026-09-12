#pragma once
// Adapted from biscuit, MIT, Copyright (c) 2025 Dave Allie.
#include <cstdint>

#include "activities/Activity.h"

class HabitTrackerActivity final : public Activity {
 public:
  explicit HabitTrackerActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("HabitTracker", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct Habit {
    char name[32]{};
    bool done = false;
    uint32_t streak = 0, best = 0;
  };
  Habit habits[10]{};
  // Fixed encoded record: header 13 + 10*41-byte habits + 4-byte checksum.
  // Member storage avoids placing 427 bytes on the task stack.
  uint8_t bytes[427]{};
  uint32_t generation = 0, session = 1;
  int count = 0, selected = 0, activeSlot = -1;
  enum class Screen { Main, Edit, NewSession, Delete };
  Screen screen = Screen::Main;
  bool dirty = false, error = false, blocked = false;
  unsigned long changedAt = 0;
  static const char* slotPath(int slot);
  static uint32_t checksum(const uint8_t* data, int size);
  uint32_t get32(int offset) const;
  void put32(int offset, uint32_t value);
  bool readSlot(int slot);
  void decode();
  void load();
  bool save();
  void changed();
  void addHabit();
};
