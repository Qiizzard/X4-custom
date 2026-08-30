#pragma once
// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
// Countdown -- tools app. Ported from biscuit through docs/merge/PORT_CHECKLIST.md.
#include <cstdint>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class CountdownActivity final : public Activity {
 public:
  explicit CountdownActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Countdown", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  // An e-ink countdown that only repaints when the display sleeps is useless,
  // so hold the device awake while the timer is actually running -- and only
  // then, so a forgotten Countdown on the picker screen still sleeps normally.
  bool preventAutoSleep() override { return state == State::Running; }

 private:
  enum class State : uint8_t { Pick, Running, Paused, Finished };

  static constexpr uint8_t kPresetCount = 7;
  // Minutes. static constexpr => flash (rule 4).
  static constexpr uint16_t kPresetMinutes[kPresetCount] = {1, 3, 5, 10, 15, 30, 60};

  State state = State::Pick;
  uint8_t presetIndex = 2;  // 5 minutes
  uint32_t remainingMs = 0;
  unsigned long lastTickMs = 0;
  // Repaint once a second while running. A full e-ink refresh per second would
  // be both slow and a battery cost, so the seconds digit drives the update and
  // nothing else does.
  uint32_t lastShownSecond = UINT32_MAX;
  ButtonNavigator buttonNavigator;

  void startSelected();
  uint32_t remainingSeconds() const { return (remainingMs + 999) / 1000; }
};
