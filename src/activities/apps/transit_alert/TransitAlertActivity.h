#pragma once
#include <RadioManager.h>

#include "activities/Activity.h"

class TransitAlertActivity final : public Activity {
 public:
  TransitAlertActivity(GfxRenderer& r, MappedInputManager& i) : Activity("TransitAlert", r, i) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return monitoring || busy; }

 private:
  struct Stop {
    char name[32]{};
    uint8_t bssids[5][6]{};
    uint8_t count = 0;
  };
  // Fixed activity-owned storage: 2,016-byte stops and 840-byte scan snapshot.
  // Too large for the task stack; allocated once with the fallible activity.
  Stop stops[32]{};
  RadioManager::ScanResult aps[20]{};
  uint8_t fingerprint[5][6]{};
  uint8_t slots[32]{};
  unsigned count = 0, selected = 0, fingerprintCount = 0, score = 0;
  uint32_t lastScan = 0, started = 0;
  bool monitoring = false, alert = false, busy = false, owned = false;
  bool failed = false, storageError = false;
  void load();
  void capture();
  bool scan();
  void stop();
};
