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
  RingBuffer<1024> incoming, relayQueue;
  struct Peer {
    uint8_t mac[6]{};
    char name[17]{};
    uint32_t seen = 0;
  } peers[16];
  uint32_t hashes[16]{}, hashTimes[16]{};
  unsigned peerCount = 0, peerSelected = 0, hashNext = 0, hashCount = 0, view = 0;
  bool relay = false;
  unsigned long lastPresence = 0, lastRelay = 0;
  void observePeer(const uint8_t*);
  bool remember(const uint8_t*);
  uint8_t packet[224]{}, mac[6]{};
  unsigned count = 0, next = 0, selected = 0, page = 0;
  bool running = false, failed = false, sent = false;
  unsigned long lastRender = 0;
  bool dirty = false;
  static void receive(void*, const uint8_t*, uint16_t);
  void append(const uint8_t*);
  void compose();
};
