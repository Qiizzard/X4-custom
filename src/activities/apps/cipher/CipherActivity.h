#pragma once
// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
// Cipher Tools -- tools app. Ported from biscuit through docs/merge/PORT_CHECKLIST.md.
// The transforms themselves live in Cipher.h so they can be host-tested; this
// is the screen around them.
#include <cstdint>

#include "Cipher.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

struct Rect;

class CipherActivity final : public Activity {
 public:
  explicit CipherActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Cipher", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class State : uint8_t { SelectAlgorithm, Result };

  // Fixed in-object buffers -- no std::string in the activity itself (rules 1
  // and 3). 128 characters is a generous sentence; the result buffer is sized
  // from cipher::outputCapacityFor() so hex/base64 growth always fits.
  static constexpr size_t kMaxInput = 128;
  static constexpr size_t kMaxKey = 32;
  static constexpr size_t kMaxResult = cipher::outputCapacityFor(kMaxInput);

  State state = State::SelectAlgorithm;
  size_t algorithmIndex = 0;
  char inputText[kMaxInput + 1] = "";
  char keyText[kMaxKey + 1] = "";
  char resultText[kMaxResult] = "";
  bool resultOverflowed = false;
  ButtonNavigator buttonNavigator;

  void beginRun();
  void promptForInput();
  void promptForKey();
  void computeResult();
  void renderSelect(const Rect& header) const;
  void renderResult(const Rect& header) const;
};
