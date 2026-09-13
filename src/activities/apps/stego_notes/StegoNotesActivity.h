#pragma once
// Adapted from Biscuit SteganographyActivity, MIT, Copyright 2025 Dave Allie.
#include "EncryptedBmp.h"
#include "activities/Activity.h"

class StegoNotesActivity final : public Activity {
 public:
  StegoNotesActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("StegoNotes", renderer, input) {}
  ~StegoNotesActivity() override;
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool allowFrontlightPanelGesture() const override { return false; }

 private:
  enum class Screen { Mode, Files, Compose, View, Saved, Error };
  enum class Input { Text, Key, ConfirmKey };
  Screen screen = Screen::Mode;
  Input inputStep = Input::Text;
  // ~2.5 KB fixed activity storage, owned by fallible launcher allocation,
  // not stack/global. Workspace is reused; no second framebuffer.
  stegobmp::Workspace workspace;
  char names[16][65]{};
  char note[stegobmp::kMaxNote + 1]{}, entry[65]{}, key[65]{};
  char source[96]{}, destination[96]{};
  size_t length = 0, page = 0;
  int selected = 0, count = 0;
  bool extract = false, limited = false;
  unsigned long lastInput = 0, revealedAt = 0;
  void wipe();
  void reset();
  void fail();
  void scan();
  void ask(Input step);
  void accept(bool cancelled);
  bool outputPath();
};
