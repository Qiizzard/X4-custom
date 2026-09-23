#pragma once
#include "activities/Activity.h"

class DnsLookupActivity final : public Activity {
 public:
  DnsLookupActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("DnsLookup", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class State { Waiting, Resolving, Result, Failed };
  State state = State::Waiting;
  bool owned = false;
  char hostname[254] = "example.com";
  char address[48] = {};
  void prompt();
  void resolve();
};
