#pragma once
#include <SecureStore.h>

#include "activities/Activity.h"

class FileCryptoActivity final : public Activity {
 public:
  FileCryptoActivity(GfxRenderer& r, MappedInputManager& i) : Activity("FileCrypto", r, i) {}
  ~FileCryptoActivity() override;
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool allowFrontlightPanelGesture() const override { return false; }

 private:
  enum class State { Mode, Files, Done, Error };
  State state = State::Mode;
  bool decrypt = false, limited = false;
  unsigned count = 0, selected = 0;
  // Fixed activity-owned working set: ~9.6 KiB, reused for one file at a time.
  uint8_t plain[4096]{}, blob[4096 + securestore::kHeaderBytes]{};
  char names[16][65]{}, entry[65]{}, key[65]{}, destination[96]{};
  void wipe();
  void fail();
  void scan();
  void ask(bool repeat);
  bool process();
};
