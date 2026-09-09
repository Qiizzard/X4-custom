#pragma once
// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
// QR Generator -- tools app. Ported from biscuit through docs/merge/PORT_CHECKLIST.md.
// Rendering reuses src/util/QrUtils.h, which this tree (CrossInk) already
// ships and already tests for the WiFi-share QR screen -- this app is a thin
// keyboard-entry shell around it, not a new QR implementation.
#include <cstdint>

#include "activities/Activity.h"

class QrGeneratorActivity final : public Activity {
 public:
  explicit QrGeneratorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("QrGenerator", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class State : uint8_t { TextInput, QrDisplay };

  // Fixed in-object buffer, not std::string (rules 1 and 3). 256 bytes is
  // enough for a URL or a WiFi/contact payload -- the practical range for
  // something a person re-types by hand off a 7-button keyboard -- while
  // keeping the activity's resident cost small and known at compile time.
  // QrUtils::drawQrCode takes a std::string by reference; a bounded
  // std::string is constructed from this buffer only at render time, the
  // same pattern CountdownActivity and others already use to bridge a fixed
  // char[] into a std::string-taking API.
  static constexpr size_t kMaxPayload = 256;

  State state = State::TextInput;
  char payload[kMaxPayload + 1] = "";
};
