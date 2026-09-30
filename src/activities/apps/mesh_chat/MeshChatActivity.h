#pragma once
#include <RingBuffer.h>

#include "activities/Activity.h"
class MeshChatActivity final : public Activity {
 public:
  MeshChatActivity(GfxRenderer& r, MappedInputManager& i) : Activity("MeshChat", r, i) {}
  void onEnter() override {
    Activity::onEnter();
    requestUpdate();
  }
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct Message {
    char name[17]{}, text[200]{};
  } messages[8];
  RingBuffer<1024> incoming;
  uint8_t packet[224]{}, mac[6]{};
  unsigned count = 0, next = 0, selected = 0, page = 0;
  bool running = false, failed = false, sent = false;
  unsigned long lastRender = 0;
  bool dirty = false;
  static void receive(void*, const uint8_t*, uint16_t);
  void append(const uint8_t*);
  void compose();
};
