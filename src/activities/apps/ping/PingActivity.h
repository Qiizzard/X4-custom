#pragma once
#include "activities/Activity.h"

class PingActivity final : public Activity {
 public:
  PingActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("PingTcp", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return state == State::Running || state == State::Resolving; }

 private:
  enum class State { Waiting, Resolving, Running, Summary, Failed };
  State state = State::Waiting;
  bool owned = false;
  char hostname[254] = "example.com";
  char address[48] = {};
  uint32_t nextProbe = 0;
  uint32_t totalMs = 0, minMs = 0, maxMs = 0, lastMs = 0;
  uint8_t attempts = 0, connected = 0;
  bool lastConnected = false;
  void prompt();
  void start();
};
