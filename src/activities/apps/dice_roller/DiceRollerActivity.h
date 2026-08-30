#pragma once
// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
// Dice Roller -- games app. Ported from biscuit through docs/merge/PORT_CHECKLIST.md.
// See PORT_NOTES.md for the gate audit.
#include <cstdint>

#include "activities/Activity.h"

class DiceRollerActivity final : public Activity {
 public:
  explicit DiceRollerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("DiceRoller", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class State : uint8_t { Select, Rolling, Result };

  // d4 through d100. static constexpr => flash, not DRAM (rule 4).
  static constexpr uint8_t kDieTypeCount = 7;
  static constexpr uint16_t kDieTypes[kDieTypeCount] = {4, 6, 8, 10, 12, 20, 100};
  static constexpr uint8_t kMaxDice = 6;
  static constexpr uint8_t kAnimFrames = 4;
  static constexpr unsigned long kAnimFrameMs = 250;

  // Fixed array, not a vector: the cap is 6 and it is known at compile time, so
  // there is nothing to allocate and nothing to grow (rules 1 and 2). biscuit's
  // original push_back'd into an unreserved std::vector on every animation frame.
  uint16_t results[kMaxDice] = {};
  uint8_t resultCount = 0;
  uint32_t total = 0;

  State state = State::Select;
  uint8_t dieTypeIndex = 1;  // d6
  uint8_t dieCount = 1;
  uint8_t animFrame = 0;
  unsigned long animStartMs = 0;

  // xorshift32, seeded from the clock on entry. A dice app wants an even
  // distribution, not unpredictability against an adversary -- SecureStore is
  // where real randomness matters, and saying so here keeps the two apart.
  uint32_t rngState = 1;
  uint16_t rollDie();
  void rollAll();

  void renderSelect() const;
  void renderResult() const;
};
