#pragma once
#include <RadioManager.h>

#include "activities/Activity.h"
class SignalLocatorActivity final : public Activity {
 public:
  SignalLocatorActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("SignalLocator", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return state == State::Measuring || busy; }

 private:
  enum class State { Select, Ready, Measuring, Results, Failed };
  State state = State::Select;
  bool owned = false, busy = false;
  int count = 0, selected = 0, point = 0;
  // Reused 1,680-byte snapshot in the fallible activity; three fixed summaries.
  RadioManager::ScanResult aps[40] = {};
  uint8_t target[6] = {};
  char targetName[33] = {};
  struct Reading {
    int16_t sum = 0;
    uint8_t seen = 0, attempts = 0;
  } readings[3];
  void scan(bool sampling);
};
