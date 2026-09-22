#pragma once

#include <RadioManager.h>

#include "activities/Activity.h"

// Bounded adaptation of Biscuit's Wi-Fi snapshot/detail view. Passive only.
class WifiScannerActivity final : public Activity {
 public:
  WifiScannerActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("WifiScanner", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // Lives in the fallibly allocated activity, never on the task stack.
  RadioManager::ScanResult results[RadioManager::kMaxScanResults] = {};
  int count = 0;
  int selected = 0;
  bool channelView = false;
  int selectedChannel = 1;
  enum class ExportStatus { None, Saved, Failed };
  ExportStatus exportStatus = ExportStatus::None;
  unsigned long exportShownAt = 0;
  int exportSlot = -1;
  bool scanning = false;
  bool owned = false;
  void scan();
  void renderChannels() const;
  bool saveCsv(int& slot) const;
};
