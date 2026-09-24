#pragma once
#include <HalStorage.h>
#include <RadioManager.h>

#include "activities/Activity.h"

class ApJournalActivity final : public Activity {
 public:
  enum class Kind { History, Wardriving, Changes, HeatMap, Watch };
  ApJournalActivity(GfxRenderer& renderer, MappedInputManager& input, Kind kind)
      : Activity("ApJournal", renderer, input), kind(kind) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return logging || busy; }

 private:
  using Ap = RadioManager::ScanResult;
  struct Record {
    Ap ap;
    uint32_t first, last, hits;
  };
  struct Difference {
    uint8_t index, kind;
  };  // 0 new, 1 absent, 2 metadata changed
  Kind kind;
  bool owned = false, busy = false, logging = false, failed = false, full = false, baseline = false;
  uint8_t interval = 0;
  int found = 0, recordCount = 0, selected = 0, differenceCount = 0;
  uint32_t scans = 0, nextScan = 0, missed = 0, rowsWritten = 0;
  int baselineCount = 0;
  // About 5.4 KiB bounded activity storage, reused across scans; no growing lists.
  Ap current[40] = {};
  Record records[64] = {};
  Difference differences[80] = {};
  char path[48] = {};
  const char* owner() const;
  uint32_t pauseMs() const;
  void updateWatch(uint32_t now);
  bool exportWatch();
  void scan(bool saveBaseline = false);
  bool createLog();
  bool appendLog(uint32_t timestamp);
  void compare();
  bool saveBaseline();
  bool loadBaseline();
};
