#pragma once
#include "activities/Activity.h"
class NetworkMonitorActivity final : public Activity {
 public:
  NetworkMonitorActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("NetworkMonitor", renderer, input) {}
  void onEnter() override {
    Activity::onEnter();
    requestUpdate();
  }
  void loop() override;
  void render(RenderLock&&) override;

 private:
  bool groups = false, failed = false;
};
