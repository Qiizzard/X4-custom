#pragma once
#include "activities/Activity.h"

class VaultDeleteActivity final : public Activity {
 public:
  VaultDeleteActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("VaultDelete", renderer, input) {}
  void onEnter() override {
    Activity::onEnter();
    requestUpdate();
  }
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class State { Review, Armed, Done };
  State state = State::Review;
  unsigned long armedAt = 0;
  unsigned removed = 0, failed = 0, notFound = 0;
  void removeVaultFiles();
};
