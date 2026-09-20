#pragma once
// RadioManager -- one owner of the antenna at a time.
//
// RULESET.md rules 7-9. The failure this exists to prevent is the one biscuit's
// audit catalogued as the state-leak class: a screen turns the radio on, exits
// without turning it off, and every later screen inherits a radio in a mode it
// did not ask for. The symptom is not a crash -- it is a scanner that returns
// nothing, a download that hangs, or an OTA that fails, one screen after the
// real bug. Ownership has to be tracked centrally or it is not tracked at all.
//
// The contract, mirroring rule 8:
//     onEnter():  if (!RADIO.acquire(Mode::WifiScan, "wifi_scanner")) { ...bail... }
//     onExit():   RADIO.shutdown();
// Acquire and release are symmetric. An app that returns from onExit() still
// holding the radio is a bug, and holdReport() exists so it is a visible one.
//
// Why WiFi and BLE contend: the C3 has one 2.4 GHz antenna and one RF front
// end. They can technically coexist, but sharing costs both of them throughput
// and adds failure modes nobody wants to debug on a device with no screen for
// logs. This firmware serialises them instead -- do your work, release.
//
// What this does NOT do yet: CrossInk's own network screens (OPDS, OTA, web
// server, nearby transfer) still drive Arduino WiFi directly, from before this
// arbiter existed. Rewriting all of them at once is a large diff through the
// reader's most load-bearing paths, so instead acquire() *detects* a radio that
// is already up and refuses rather than stomping on it. New apps go through
// RADIO; the legacy call sites migrate one reviewed change at a time.
// See docs/merge/RADIO_MIGRATION.md.

#include <cstddef>
#include <cstdint>

class RadioManager {
 public:
  enum class Mode : uint8_t {
    Off,              // radio down; the state every screen should leave behind
    WifiStation,      // associated to an AP (downloads, OPDS, sync)
    WifiScan,         // station mode, scanning only, never associated
    WifiPromiscuous,  // monitor mode: raw frames, listen only
    EspNow,           // device-to-device, no AP
    WifiAccessPoint,  // configure through acquireAccessPoint(), not acquire()
  };

  // Raw-frame sink for promiscuous mode. A plain function pointer plus a
  // context, deliberately not std::function: this is called once per frame in
  // the air, and rule 10 forbids allocation on that path.
  //
  // The callee must copy the bytes it wants into a pre-allocated RingBuffer and
  // return. No parsing, no sorting, no std::string, no SD access -- all of that
  // belongs in loop().
  using FrameSink = void (*)(void* context, const uint8_t* frame, uint16_t length, int8_t rssi, uint8_t channel);

  static constexpr uint8_t kMinChannel = 1;
  static constexpr uint8_t kMaxChannel = 13;
  // Hard ceiling on a scan result set, so a dense environment cannot grow an
  // unbounded list on a 380 KB device (rule 1).
  static constexpr size_t kMaxScanResults = 40;

  struct ScanResult {
    char ssid[33];  // 32 bytes + terminator; SSIDs are not null-terminated on the wire
    uint8_t bssid[6];
    int8_t rssi;
    uint8_t channel;
    bool encrypted;
  };

  static RadioManager& instance();

  // Take the radio for `owner` (a short static string used in logs; it must
  // outlive the hold -- pass a literal). Fails if someone else holds it, or if
  // a legacy path already brought the radio up.
  // Re-acquiring the same mode from the same owner is a no-op success, so an
  // activity that re-enters does not have to track whether it already holds it.
  bool acquire(Mode mode, const char* owner);
  // Dedicated AP startup validates configuration before touching the driver.
  // nullptr password explicitly requests an open AP; non-null must be 8–63
  // printable ASCII bytes. SSID 1–32 bytes, channel 1–13, clients 1–4.
  // No credentials retained. Existing managed/foreign sessions are refused.
  bool acquireAccessPoint(const char* owner, const char* ssid, const char* password, uint8_t channel,
                          uint8_t maxClients);
  // Write the active owned AP's address, or zero output on failure.
  bool accessPointAddress(const char* owner, uint8_t (&address)[4]) const;

  // Release and take the radio all the way down. Safe to call when nothing is
  // held, so it can sit unconditionally in onExit(). This is the only correct
  // way to end a hold -- there is no "release but leave it on" variant, because
  // that is the state leak this class exists to prevent.
  void shutdown();
  // Activity-facing release: a failed acquire must never tear down another owner.
  // Use the identical static owner pointer passed to acquire().
  bool shutdown(const char* owner);
  // Connected with a nonzero station IP; excludes association before DHCP.
  // Configure an owned ESP-NOW radio before the activity initializes its protocol.
  bool configureEspNow(const char* owner, uint8_t channel);
  bool stationConnected(const char* owner) const;
  int stationRssi(const char* owner) const;  // -127 when not owned/connected

  bool isHeld() const { return mode_ != Mode::Off; }
  Mode mode() const { return mode_; }
  // Never null: "none" when unheld.
  const char* owner() const { return owner_ != nullptr ? owner_ : "none"; }

  // True when something outside RadioManager has the radio up (a legacy
  // CrossInk network screen). Apps should surface this as "busy", not retry.
  static bool foreignRadioActive();

  // Scan for access points into `out`, capped at `capacity` (never more than
  // kMaxScanResults). Requires a WifiScan or WifiStation hold. Returns the
  // number written, or -1 on failure. Blocking; expect a couple of seconds.
  int scanNetworks(ScanResult* out, size_t capacity);

  // Transitional picker API: a static parent token requires its station hold.
  // nullptr is allowed only with no managed hold, for unmigrated legacy parents.
  // Results are capped at kMaxScanResults; the SDK's internal scan allocation is not.
  static constexpr int kScanRunning = -1;
  static constexpr int kScanFailed = -2;
  int startPickerScan(const char* owner);
  int pickerScanCount(const char* owner);
  bool pickerScanResult(const char* owner, size_t index, ScanResult& out);
  void clearPickerScan(const char* owner);
  // Same transitional authorization as scans. No credentials retained here.
  bool preparePickerConnection(const char* owner);
  int beginPickerConnection(const char* owner, const char* ssid, const char* password);
  // finish=true also powers off legacy sessions; managed parents retain their hold.
  void disconnectPicker(const char* owner, bool finish = false);

  // Enter monitor mode and route frames to `sink`. Requires a WifiPromiscuous
  // hold. LISTEN ONLY -- nothing in this firmware transmits a crafted frame.
  bool startPromiscuous(FrameSink sink, void* context, uint8_t channel);
  // Retune while capturing, for channel-hopping captures.
  bool setChannel(uint8_t channel);
  uint8_t channel() const { return channel_; }
  void stopPromiscuous();

  // Milliseconds the current hold has lasted, or 0 when unheld. Rule 9 says do
  // your work and release; a UI can use this to show a long hold.
  uint32_t heldForMs() const;

  // One-line description of the current hold for logs and the Task Manager
  // screen, e.g. "wifi_scanner holds WifiScan for 3200ms". Written into
  // `buffer`; never allocates.
  void holdReport(char* buffer, size_t bufferSize) const;

  static const char* modeName(Mode mode);

 private:
  RadioManager() = default;
  RadioManager(const RadioManager&) = delete;
  RadioManager& operator=(const RadioManager&) = delete;

  bool pickerAccessAllowed(const char* owner) const;
  bool startWifi(Mode mode);
  void stopWifi();

  Mode mode_ = Mode::Off;
  const char* owner_ = nullptr;
  uint32_t acquiredAtMs_ = 0;
  uint8_t channel_ = kMinChannel;
  bool promiscuousActive_ = false;
};

// The one handle app code uses, so a grep for `RADIO.` finds every radio user.
#define RADIO RadioManager::instance()
