#pragma once
#include "activities/Activity.h"

class HostScannerActivity final : public Activity {
 public:
  HostScannerActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("HostScanner", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return state == State::Scanning || state == State::Ports; }

 private:
  enum class State { Waiting, Ready, Scanning, Hosts, Ports, Failed };
  State state = State::Waiting;
  bool owned = false, partial = false;
  struct Host {
    uint32_t ip;
    uint16_t open;
    uint16_t tested;
  };
  // 256 bytes owned by the fallibly allocated activity, reused for each scan.
  Host hosts[32] = {};
  uint32_t first = 0, last = 0, self = 0, next = 0;
  int count = 0, selected = 0, portIndex = 0;
  int exportStatus = 0, exportSlot = -1;
  void startScan();
  void step();
  bool saveCsv(int& slot) const;
};
