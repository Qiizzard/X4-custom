#pragma once
#include <HalStorage.h>
#include <RingBuffer.h>

#include "activities/Activity.h"

class PassiveMonitorActivity final : public Activity {
 public:
  enum class Kind { Packets, Probes, Deauth };
  PassiveMonitorActivity(GfxRenderer& renderer, MappedInputManager& input, Kind kind)
      : Activity("PassiveMonitor", renderer, input), kind(kind) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return running; }

 private:
  struct Packet {
    uint16_t original;
    int8_t rssi;
    uint8_t channel;
    uint8_t bytes[96];
  };
  struct Event {
    uint8_t source[6], destination[6];
    char ssid[33];
    uint16_t reason;
    int8_t rssi;
    uint8_t channel, subtype;
    bool protectedFrame, reasonKnown;
  };
  Kind kind;
  bool owned = false, running = false, failed = false, hopping = false;
  uint8_t channel = 1, eventHead = 0, eventCount = 0, selected = 0;
  uint32_t total = 0, management = 0, dataFrames = 0, control = 0, probes = 0, deauth = 0, disassoc = 0;
  uint32_t lastRefresh = 0, lastHop = 0;
  // Fixed callback queue/scratch/history, about 2.6 KiB, owned by fallible activity.
  RingBuffer<2048> packets;
  Packet scratch = {};
  Event events[8] = {};
  HalFile capture;
  char capturePath[48] = {};
  uint32_t captureBytes = 0, syncedBytes = 0;
  int captureStatus = 0;  // 0 off, 1 recording, 2 saved, 3 size cap, -1 failure
  static void receive(void*, const uint8_t*, uint16_t, int8_t, uint8_t);
  const char* owner() const;
  void process(const Packet& packet);
  void toggleCapture();
  void closeCapture(int status);
  void writeCapture(const Packet& packet);
};
