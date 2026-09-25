#pragma once
#include "activities/Activity.h"
class ScreenDecoyActivity final : public Activity {
 public:
  ScreenDecoyActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("ScreenDecoy", renderer, input) {}
  void onEnter() override {
    Activity::onEnter();
    requestUpdate();
  }
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class State { Select, Preview, Active };
  State state = State::Select;
  unsigned selected = 0;
};
