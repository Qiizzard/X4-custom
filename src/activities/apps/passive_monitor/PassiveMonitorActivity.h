#pragma once
#include <HalStorage.h>
#include <RingBuffer.h>

#include "activities/Activity.h"

class PassiveMonitorActivity final : public Activity {
 public:
  enum class Kind { Packets, Probes, Deauth, Crowd, Fingerprint };
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
  struct Summary {
    Event event;
    uint8_t bssid[6];
    uint32_t count, first, last;
  };
  Summary summaries[24] = {};
  uint8_t summaryCount = 0;
  uint32_t skippedEvents = 0, rateCount = 0;
  struct Rate {
    uint32_t count, elapsed;
  };
  Rate rates[40] = {};
  uint8_t rateHead = 0, rateSize = 0;
  void renderRates(int top);
  Kind kind;
  bool owned = false, running = false, failed = false, hopping = false;
  uint8_t channel = 1, eventHead = 0, eventCount = 0, selected = 0;
  uint32_t total = 0, management = 0, dataFrames = 0, control = 0, probes = 0, deauth = 0, disassoc = 0;
  uint32_t lastRefresh = 0, lastHop = 0;
  struct Peer {
    uint8_t mac[6];
    char ssid[33];
    int8_t rssi;
    uint8_t channel;
    uint32_t frames;
  };
  // Fixed 24-row summary (1,152 bytes); no growing MAC/SSID collections.
  Peer peers[24] = {};
  uint8_t peerCount = 0;
  uint32_t untracked = 0, channelFrames[14] = {};
  // Sixty fixed observation windows (720 bytes), never a people estimate.
  struct Window {
    uint32_t elapsed, skipped;
    uint8_t count;
  };
  Window windows[60] = {};
  uint8_t windowHead = 0, windowCount = 0;
  bool chart = false, spike = false;
  uint32_t intervalStart = 0, intervalCount = 0, spikeFrames = 0, spikeElapsed = 0;
  int csvStatus = 0;
  char csvPath[48] = {};
  void track(const uint8_t* mac, const char* ssid, int8_t rssi, uint8_t channel);
  bool saveCsv();
  void renderChannels(int top);
  bool probeView() const { return kind == Kind::Probes || kind == Kind::Fingerprint; }
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
