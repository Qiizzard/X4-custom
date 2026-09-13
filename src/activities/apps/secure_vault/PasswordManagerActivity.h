#pragma once
// UI flow adapted from Biscuit, MIT, Copyright (c) 2025 Dave Allie.
#include "PasswordRecords.h"
#include "activities/Activity.h"

class PasswordManagerActivity final : public Activity {
 public:
  PasswordManagerActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("PasswordManager", renderer, input) {}
  ~PasswordManagerActivity() override;
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool allowFrontlightPanelGesture() const override { return false; }

 private:
  enum class Screen { Locked, List, Detail, Delete, Save, Error };
  enum class Input { Unlock, Create, ConfirmKey, Title, Username, Password };
  Screen screen = Screen::Locked;
  Input inputStep = Input::Unlock;
  PasswordRecords records;
  PasswordRecords::Record draft;
  // Activity-owned fixed working set (~3.8 KB), never on the task stack or global.
  uint8_t encoded[PasswordRecords::kEncodedBytes]{};
  uint8_t scratch[PasswordRecords::kEncodedBytes + 60]{};
  char passphrase[65]{}, entry[65]{};
  size_t selected = 0;
  unsigned long lastInput = 0, revealedAt = 0;
  bool existing = false, reveal = false;
  void wipe();
  void fail();
  void ask(Input step);
  void accept(bool cancelled);
  bool save(bool creating);
};
