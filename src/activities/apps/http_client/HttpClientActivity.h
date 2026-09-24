#pragma once
#include "activities/Activity.h"
#include "network/NetworkToolRequest.h"

class HttpClientActivity final : public Activity {
 public:
  HttpClientActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("HttpClient", renderer, input) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return state == State::Working; }

 private:
  enum class State { Waiting, Menu, Working, Result, Failed };
  State state = State::Waiting;
  bool owned = false, post = false;
  int selected = 0;
  size_t offset = 0, pageBytes = 1;
  // About 1.8 KiB fixed input/preview storage in the fallible activity allocation.
  char url[257] = "https://example.com/";
  char body[513] = {};
  NetworkToolRequest::Result response;
  void edit(bool bodyField);
  void send();
};
