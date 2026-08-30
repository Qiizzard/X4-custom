#pragma once
// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
// Unit Converter -- tools app. Ported from biscuit through docs/merge/PORT_CHECKLIST.md.
// The tables and maths live in UnitConversion.h so they can be host-tested.
#include <cstdint>

#include "UnitConversion.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class UnitConverterActivity final : public Activity {
 public:
  explicit UnitConverterActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("UnitConverter", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class State : uint8_t { PickCategory, PickUnit, ShowResults };

  // Widest category is Length at 8 units, so a fixed row buffer covers every
  // case with no allocation (rules 1 and 3). biscuit built a
  // std::vector<std::string> of results on every recompute.
  static constexpr size_t kMaxUnits = 8;
  static constexpr size_t kRowTextLength = 40;

  State state = State::PickCategory;
  size_t categoryIndex = 0;
  size_t unitIndex = 0;
  int selectedIndex = 0;
  double inputValue = 1.0;
  char inputText[24] = "1";
  char rows[kMaxUnits][kRowTextLength] = {};
  size_t rowCount = 0;
  ButtonNavigator buttonNavigator;

  size_t itemCount() const;
  void computeRows();
  void promptForValue();
};
