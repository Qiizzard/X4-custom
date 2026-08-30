#include "RadioManager.h"

#include <Arduino.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>

#ifndef SIMULATOR
#include <WiFi.h>
#include <esp_wifi.h>
#endif

namespace {
constexpr const char* TAG = "RADIO";
}  // namespace

RadioManager& RadioManager::instance() {
  static RadioManager manager;
  return manager;
}

const char* RadioManager::modeName(const Mode mode) {
  switch (mode) {
    case Mode::Off:
      return "Off";
    case Mode::WifiStation:
      return "WifiStation";
    case Mode::WifiScan:
      return "WifiScan";
    case Mode::WifiPromiscuous:
      return "WifiPromiscuous";
    case Mode::EspNow:
      return "EspNow";
  }
  return "?";
}

uint32_t RadioManager::heldForMs() const {
  if (mode_ == Mode::Off) return 0;
  return static_cast<uint32_t>(millis()) - acquiredAtMs_;
}

void RadioManager::holdReport(char* buffer, const size_t bufferSize) const {
  if (buffer == nullptr || bufferSize == 0) return;
  if (mode_ == Mode::Off) {
    snprintf(buffer, bufferSize, "radio idle");
    return;
  }
  snprintf(buffer, bufferSize, "%s holds %s for %lums", owner(), modeName(mode_),
           static_cast<unsigned long>(heldForMs()));
}

#ifdef SIMULATOR

// The simulator has no radio. Everything fails cleanly and says why, so a radio
// app built for -e simulator renders its "unavailable" state instead of
// pretending to scan -- the same convention CrossInk's own nearby-sync screens
// already use.

bool RadioManager::foreignRadioActive() { return false; }

bool RadioManager::acquire(const Mode mode, const char* owner) {
  LOG_INF(TAG, "simulator build: %s denied %s (no radio)", owner != nullptr ? owner : "?", modeName(mode));
  return false;
}

void RadioManager::shutdown() {
  mode_ = Mode::Off;
  owner_ = nullptr;
  promiscuousActive_ = false;
}

int RadioManager::scanNetworks(ScanResult*, size_t) { return -1; }
bool RadioManager::startPromiscuous(FrameSink, void*, uint8_t) { return false; }
bool RadioManager::setChannel(uint8_t) { return false; }
void RadioManager::stopPromiscuous() {}
bool RadioManager::startWifi(Mode) { return false; }
void RadioManager::stopWifi() {}

#else

namespace {

// Promiscuous frames arrive on a WiFi-task callback, not the activity task, so
// the sink and its context have to be reachable from there. File-scope pointers
// are the simplest thing that is correct: they are written only while the radio
// is stopped and read only while it is running.
RadioManager::FrameSink g_sink = nullptr;
void* g_sinkContext = nullptr;

// Trampoline from the WiFi driver's promiscuous callback to the app's sink.
// RULESET rule 10: this function allocates nothing, parses nothing, and does no
// SD or string work. It reads a header, hands over a pointer and a length, and
// returns. Anything slower here drops frames or trips the watchdog.
void promiscuousTrampoline(void* buf, const wifi_promiscuous_pkt_type_t type) {
  if (g_sink == nullptr || buf == nullptr) return;
  // Control frames carry no payload worth capturing and arrive in floods.
  if (type == WIFI_PKT_MISC) return;
  const auto* packet = static_cast<const wifi_promiscuous_pkt_t*>(buf);
  const uint16_t length = static_cast<uint16_t>(packet->rx_ctrl.sig_len);
  if (length == 0) return;
  g_sink(g_sinkContext, packet->payload, length, static_cast<int8_t>(packet->rx_ctrl.rssi),
         static_cast<uint8_t>(packet->rx_ctrl.channel));
}

}  // namespace

bool RadioManager::foreignRadioActive() {
  // A legacy CrossInk screen brought WiFi up without going through here.
  return WiFi.getMode() != WIFI_MODE_NULL;
}

bool RadioManager::acquire(const Mode mode, const char* owner) {
  if (mode == Mode::Off) {
    LOG_ERR(TAG, "acquire(Off) is not a thing -- call shutdown()");
    return false;
  }
  if (owner == nullptr) {
    LOG_ERR(TAG, "acquire refused: every hold must name an owner");
    return false;
  }

  // Re-acquiring what you already hold is fine; it keeps re-entrant onEnter()
  // paths from having to track their own state.
  if (mode_ == mode && owner_ == owner) {
    return true;
  }

  if (isHeld()) {
    // Refusing is right: silently taking the antenna from a live capture or an
    // in-flight download is exactly the state leak this class prevents.
    LOG_ERR(TAG, "%s denied %s: %s already holds %s (%lums)", owner, modeName(mode), owner_, modeName(mode_),
            static_cast<unsigned long>(heldForMs()));
    return false;
  }

  if (foreignRadioActive()) {
    LOG_ERR(TAG, "%s denied %s: radio already up outside RadioManager (legacy network screen)", owner, modeName(mode));
    return false;
  }

  if (!startWifi(mode)) {
    LOG_ERR(TAG, "%s could not start %s", owner, modeName(mode));
    stopWifi();
    return false;
  }

  mode_ = mode;
  owner_ = owner;
  acquiredAtMs_ = static_cast<uint32_t>(millis());
  LOG_INF(TAG, "%s acquired %s", owner, modeName(mode));
  return true;
}

void RadioManager::shutdown() {
  if (!isHeld()) return;
  const char* previousOwner = owner();
  const Mode previousMode = mode_;
  const uint32_t held = heldForMs();

  if (promiscuousActive_) stopPromiscuous();
  stopWifi();

  mode_ = Mode::Off;
  owner_ = nullptr;
  acquiredAtMs_ = 0;
  LOG_INF(TAG, "%s released %s after %lums", previousOwner, modeName(previousMode), static_cast<unsigned long>(held));
}

bool RadioManager::startWifi(const Mode mode) {
  switch (mode) {
    case Mode::WifiStation:
    case Mode::WifiScan:
    case Mode::EspNow:
      // Station mode is the base for all three. Scanning and ESP-NOW simply
      // never associate.
      if (!WiFi.mode(WIFI_MODE_STA)) return false;
      WiFi.disconnect(false, false);
      return true;
    case Mode::WifiPromiscuous:
      // Monitor mode needs the driver up but the station logic out of the way,
      // or the supplicant fights the channel setting.
      if (!WiFi.mode(WIFI_MODE_STA)) return false;
      WiFi.disconnect(false, false);
      return esp_wifi_set_promiscuous(false) == ESP_OK;
    case Mode::Off:
      return false;
  }
  return false;
}

void RadioManager::stopWifi() {
  // Take it all the way down. Leaving the driver in STA-idle is what makes the
  // *next* screen's radio behave unpredictably.
  esp_wifi_set_promiscuous(false);
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_MODE_NULL);
}

int RadioManager::scanNetworks(ScanResult* out, const size_t capacity) {
  if (out == nullptr || capacity == 0) return -1;
  if (mode_ != Mode::WifiScan && mode_ != Mode::WifiStation) {
    LOG_ERR(TAG, "scanNetworks needs a WifiScan hold (mode is %s)", modeName(mode_));
    return -1;
  }

  const int found = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/true);
  if (found < 0) {
    LOG_ERR(TAG, "scan failed (%d)", found);
    return -1;
  }

  // Rule 1: the cap is applied before anything is written, not after.
  const size_t limit = capacity < kMaxScanResults ? capacity : kMaxScanResults;
  const size_t count = static_cast<size_t>(found) < limit ? static_cast<size_t>(found) : limit;
  for (size_t i = 0; i < count; ++i) {
    ScanResult& entry = out[i];
    // SSIDs are not null-terminated on the wire and may be up to 32 bytes.
    const String ssid = WiFi.SSID(static_cast<int>(i));
    std::snprintf(entry.ssid, sizeof(entry.ssid), "%s", ssid.c_str());
    const uint8_t* bssid = WiFi.BSSID(static_cast<int>(i));
    if (bssid != nullptr) {
      std::memcpy(entry.bssid, bssid, sizeof(entry.bssid));
    } else {
      std::memset(entry.bssid, 0, sizeof(entry.bssid));
    }
    entry.rssi = static_cast<int8_t>(WiFi.RSSI(static_cast<int>(i)));
    entry.channel = static_cast<uint8_t>(WiFi.channel(static_cast<int>(i)));
    entry.encrypted = WiFi.encryptionType(static_cast<int>(i)) != WIFI_AUTH_OPEN;
  }

  if (static_cast<size_t>(found) > count) {
    LOG_INF(TAG, "scan found %d APs, reporting %u (cap)", found, static_cast<unsigned>(count));
  }
  // Free the driver's own result list straight away; it is heap we do not need.
  WiFi.scanDelete();
  return static_cast<int>(count);
}

bool RadioManager::startPromiscuous(const FrameSink sink, void* context, const uint8_t channel) {
  if (sink == nullptr) {
    LOG_ERR(TAG, "startPromiscuous needs a sink");
    return false;
  }
  if (mode_ != Mode::WifiPromiscuous) {
    LOG_ERR(TAG, "startPromiscuous needs a WifiPromiscuous hold (mode is %s)", modeName(mode_));
    return false;
  }
  if (channel < kMinChannel || channel > kMaxChannel) {
    LOG_ERR(TAG, "channel %u out of range", static_cast<unsigned>(channel));
    return false;
  }

  // Publish the sink before enabling the callback, so no frame can arrive while
  // g_sink is still null.
  g_sink = sink;
  g_sinkContext = context;

  if (esp_wifi_set_promiscuous_rx_cb(promiscuousTrampoline) != ESP_OK) {
    LOG_ERR(TAG, "could not install the promiscuous callback");
    g_sink = nullptr;
    g_sinkContext = nullptr;
    return false;
  }
  if (esp_wifi_set_promiscuous(true) != ESP_OK) {
    LOG_ERR(TAG, "could not enter promiscuous mode");
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    g_sink = nullptr;
    g_sinkContext = nullptr;
    return false;
  }

  promiscuousActive_ = true;
  if (!setChannel(channel)) {
    stopPromiscuous();
    return false;
  }
  LOG_INF(TAG, "%s listening on channel %u", owner(), static_cast<unsigned>(channel));
  return true;
}

bool RadioManager::setChannel(const uint8_t channel) {
  if (channel < kMinChannel || channel > kMaxChannel) return false;
  if (!promiscuousActive_) {
    LOG_ERR(TAG, "setChannel outside a capture");
    return false;
  }
  if (esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    LOG_ERR(TAG, "could not tune channel %u", static_cast<unsigned>(channel));
    return false;
  }
  channel_ = channel;
  return true;
}

void RadioManager::stopPromiscuous() {
  if (!promiscuousActive_) return;
  // Stop delivery before dropping the sink, so the trampoline can never run
  // against a stale context pointer.
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  g_sink = nullptr;
  g_sinkContext = nullptr;
  promiscuousActive_ = false;
}

#endif  // SIMULATOR
