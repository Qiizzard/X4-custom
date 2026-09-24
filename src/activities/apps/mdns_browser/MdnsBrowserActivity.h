#pragma once
#include <RadioManager.h>

#include "activities/Activity.h"

class MdnsBrowserActivity final : public Activity {
 public:
  MdnsBrowserActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("MdnsBrowser", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class State { Waiting, Select, Querying, Results, Failed };
  State state = State::Waiting;
  bool owned = false;
  enum class ExportStatus { None, Saved, Failed };
  ExportStatus exportStatus = ExportStatus::None;
  int exportSlot = -1;
  int service = 0;
  int selected = 0;
  int count = 0;
  // 1,568 bytes in the fallibly allocated activity, not on a task stack.
  RadioManager::MdnsResult results[RadioManager::kMaxMdnsResults] = {};
  void query();
  bool saveCsv(int& slot) const;
};
