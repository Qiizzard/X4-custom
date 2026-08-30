#pragma once
// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
// Morse Code -- tools app. Ported from biscuit through docs/merge/PORT_CHECKLIST.md.
// The encode/decode itself lives in MorseCode.h so it can be host-tested; this
// is the screen around it.
#include <cstdint>

#include "MorseCode.h"
#include "activities/Activity.h"

class MorseCodeActivity final : public Activity {
 public:
  explicit MorseCodeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("MorseCode", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Mode : uint8_t { TextToMorse, MorseToText };

  // Fixed in-object buffers, sized once. No std::string, nothing to grow
  // (rules 1 and 3). 64 input characters is a sentence, which is what this app
  // is for; the output buffer is sized from the worst-case encoding of that.
  static constexpr size_t kMaxInput = 64;
  static constexpr size_t kMaxOutput = morse::encodedCapacityFor(kMaxInput);

  char input[kMaxInput + 1] = "";
  char output[kMaxOutput] = "";
  Mode mode = Mode::TextToMorse;
  bool overflowed = false;

  void recompute();
  void promptForInput();
};
