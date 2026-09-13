#pragma once
// UI flow adapted from Biscuit, MIT, Copyright (c) 2025 Dave Allie.
#include <qrcode.h>

#include "PasswordRecords.h"
#include "Totp.h"
#include "activities/Activity.h"

class PasswordManagerActivity final : public Activity {
 public:
  enum class Mode { Passwords, Authenticator, TotpQr };
  PasswordManagerActivity(GfxRenderer& renderer, MappedInputManager& input, Mode mode = Mode::Passwords)
      : Activity("SecureVault", renderer, input), mode(mode) {}
  ~PasswordManagerActivity() override;
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool allowFrontlightPanelGesture() const override { return false; }

 private:
  enum class Screen { Locked, List, Detail, Delete, Save, Error };
  enum class Input { Unlock, Create, ConfirmKey, Title, Username, Password };
  const Mode mode;
  Totp totp;
  char code[7]{};
  uint8_t qrModules[56]{};  // QR version 1: ceil(21*21/8), no second framebuffer
  QRCode qr{};
  uint64_t codeCounter = 0;
  unsigned long lastClockPoll = 0;
  bool codeValid = false;
  const char* path() const;
  const char* nextPath() const;
  const char* previousPath() const;
  bool unlockTotp();
  void refreshCode();
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
