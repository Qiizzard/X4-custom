#pragma once
#include <MemoryStats.h>

#include "activities/Activity.h"
class TaskManagerActivity final : public Activity {
 public:
  TaskManagerActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("TaskManager", renderer, input) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  unsigned page = 0;
  uint32_t refreshed = 0, heldMs = 0;
  bool sdReady = false, radioHeld = false;
  char owner[48] = {};
  MemoryStats::Snapshot memory;
  void refresh();
};
