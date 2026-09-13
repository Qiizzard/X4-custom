#pragma once
#include <I18nKeys.h>

#include "activities/Activity.h"

// Caller-owned destination must remain alive until this child is destroyed.
// Result conveys only cancellation; no secret is placed in ActivityResult.
// Caller wipes destination on lock/exit and must not copy it into std::string.
class SecretEntryActivity final : public Activity {
 public:
  SecretEntryActivity(GfxRenderer& renderer, MappedInputManager& input, StrId prompt, char* destination,
                      size_t capacity, size_t minimum)
      : Activity("SecretEntry", renderer, input),
        prompt(prompt),
        destination(destination),
        capacity(capacity),
        minimum(minimum) {}
  bool allowFrontlightPanelGesture() const override { return false; }
  ~SecretEntryActivity() override;
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  StrId prompt;
  char* destination;
  size_t capacity, minimum;
  char draft[65]{};  // fixed printable ASCII input, up to 64 bytes
  size_t length = 0;
  char candidate = 'A';
  unsigned long lastInput = 0;
  bool ready = false;
  void cancel();
};
