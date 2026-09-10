#pragma once
// Key-type ranges adapted from biscuit, MIT, Copyright (c) 2025 Dave Allie.
#include "activities/Activity.h"
class KeyCopierActivity final : public Activity {
 public:
  explicit KeyCopierActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("KeyCopier", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  int typeIndex = 0;
  static constexpr int TYPE_COUNT = 5;
};
