#pragma once
#include <HalBulletinServer.h>

#include "activities/Activity.h"
class BulletinBoardActivity final : public Activity {
 public:
  BulletinBoardActivity(GfxRenderer& r, MappedInputManager& i) : Activity("BulletinBoard", r, i) {}
  void onEnter() override {
    Activity::onEnter();
    requestUpdate();
  }
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  HalBulletinServer server;
  char password[64]{};
  uint8_t address[4]{};
  unsigned duration = 0, finalCount = 0;
  uint32_t started = 0, refreshed = 0;
  bool running = false, failed = false, ended = false;
  static constexpr unsigned minutes[] = {5, 15, 30};
  void start();
  void stop();
};
