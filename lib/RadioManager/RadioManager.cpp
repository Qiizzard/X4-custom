#include "RadioManager.h"

#include <Arduino.h>
#include <Logging.h>

#include <atomic>
#include <cstdio>
#include <cstring>

#ifndef SIMULATOR
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <lwip/ip6_addr.h>
#include <mdns.h>
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
    case Mode::WifiAccessPoint:
      return "WifiAccessPoint";
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

bool RadioManager::shutdown(const char* owner) {
  if (!owner) {
    LOG_ERR(TAG, "Release refused: missing owner");
    return false;
  }
  if (!isHeld()) return true;
  if (owner_ != owner) {
    LOG_ERR(TAG, "%s denied release: owned by %s", owner, this->owner());
    return false;
  }
  shutdown();
  return true;
}

bool RadioManager::pickerAccessAllowed(const char* owner) const {
  const bool allowed = owner && owner_ == owner && mode_ == Mode::WifiStation;
  if (!allowed) LOG_ERR(TAG, "Picker operation denied: owner mismatch");
  return allowed;
}

#ifdef SIMULATOR

// The simulator has no radio. Everything fails cleanly and says why, so a radio
// app built for -e simulator renders its "unavailable" state instead of
// pretending to scan -- the same convention CrossInk's own nearby-sync screens
// already use.

bool RadioManager::acquireAccessPoint(const char* owner, const char*, const char*, uint8_t, uint8_t) {
  LOG_INF(TAG, "simulator: %s denied AP (no radio)", owner ? owner : "?");
  return false;
}
bool RadioManager::accessPointAddress(const char*, uint8_t (&address)[4]) const {
  memset(address, 0, sizeof(address));
  return false;
}
bool RadioManager::configureEspNow(const char*, uint8_t) { return false; }
bool RadioManager::resolveHostname(const char*, const char*, char (&address)[48]) {
  address[0] = 0;
  return false;
}
int RadioManager::browseMdns(const char*, const char*, MdnsResult*, size_t) { return -1; }
bool RadioManager::foreignRadioActive() { return false; }
bool RadioManager::stationConnected(const char*) const { return false; }
int RadioManager::stationRssi(const char*) const { return -127; }

bool RadioManager::acquire(const Mode mode, const char* owner) {
  LOG_INF(TAG, "simulator build: %s denied %s (no radio)", owner != nullptr ? owner : "?", modeName(mode));
  return false;
}

void RadioManager::shutdown() {
  mode_ = Mode::Off;
  owner_ = nullptr;
  promiscuousActive_ = false;
}

int RadioManager::scanNetworks(ScanResult*, size_t, bool) { return -1; }
int RadioManager::startPickerScan(const char*) { return kScanFailed; }
int RadioManager::pickerScanCount(const char*) { return kScanFailed; }
bool RadioManager::pickerScanResult(const char*, size_t, ScanResult& out) {
  out = {};
  return false;
}
void RadioManager::clearPickerScan(const char*) {}
bool RadioManager::preparePickerConnection(const char*) { return false; }
int RadioManager::beginPickerConnection(const char*, const char*, const char*) { return -1; }
void RadioManager::disconnectPicker(const char*, bool) {}
bool RadioManager::pickerStatus(const char*, PickerStatus& out) const {
  out = {};
  return false;
}
bool RadioManager::stationMac(uint8_t (&mac)[6]) {
  memset(mac, 0, sizeof(mac));
  return false;
}
void RadioManager::setPickerEventLogging(const char*, bool, bool) {}
void RadioManager::logPickerDisconnectReason(const char*) const {}

bool RadioManager::startPromiscuous(FrameSink, void*, uint8_t) { return false; }
bool RadioManager::setChannel(uint8_t) { return false; }
void RadioManager::stopPromiscuous() {}
bool RadioManager::startWifi(Mode) { return false; }
void RadioManager::stopWifi() {}

#else

namespace {
std::atomic<uint8_t> sPickerDisconnectReason{0};
std::atomic<bool> sPickerLoggingActive{false};
bool sPickerEventsRegistered = false;

void logWifiStationEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (!sPickerLoggingActive) {
    return;
  }

  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      LOG_INF("WIFI", "STA event: connected to AP");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP: {
      const uint8_t* ip = reinterpret_cast<const uint8_t*>(&info.got_ip.ip_info.ip.addr);
      LOG_INF("WIFI", "STA event: got IP %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
      break;
    }
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
      uint8_t reason = info.wifi_sta_disconnected.reason;
      if (reason == 0) {
        reason = WIFI_REASON_UNSPECIFIED;
      }
      sPickerDisconnectReason = reason;
      LOG_INF("WIFI", "STA event: disconnected reason=%u(%s)", reason,
              WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(reason)));
      break;
    }
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      LOG_INF("WIFI", "STA event: lost IP");
      break;
    default:
      break;
  }
}

const char* wifiStatusName(const wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:
      return "IDLE";
    case WL_NO_SSID_AVAIL:
      return "NO_SSID_AVAIL";
    case WL_CONNECTED:
      return "CONNECTED";
    case WL_CONNECT_FAILED:
      return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:
      return "CONNECTION_LOST";
    case WL_DISCONNECTED:
      return "DISCONNECTED";
    case WL_NO_SHIELD:
      return "NO_SHIELD";
    case WL_STOPPED:
      return "STOPPED";
    case WL_SCAN_COMPLETED:
      return "SCAN_COMPLETED";
    default:
      return "UNKNOWN";
  }
}

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

bool RadioManager::stationConnected(const char* owner) const {
  return owner && owner_ == owner && mode_ == Mode::WifiStation && WiFi.status() == WL_CONNECTED &&
         WiFi.localIP() != IPAddress(0, 0, 0, 0);
}

int RadioManager::stationRssi(const char* owner) const { return stationConnected(owner) ? WiFi.RSSI() : -127; }

bool RadioManager::acquireAccessPoint(const char* owner, const char* ssid, const char* password, uint8_t channel,
                                      uint8_t maxClients) {
  const size_t ssidLength = ssid ? strnlen(ssid, 33) : 0;
  const size_t passwordLength = password ? strnlen(password, 64) : 0;
  bool validPassword = !password || (passwordLength >= 8 && passwordLength <= 63);
  if (password && validPassword) {
    for (size_t i = 0; i < passwordLength; ++i) {
      const unsigned char c = password[i];
      if (c < 32 || c > 126) validPassword = false;
    }
  }
  if (!owner || !owner[0] || !ssidLength || ssidLength > 32 || !validPassword || channel < kMinChannel ||
      channel > kMaxChannel || !maxClients || maxClients > 4) {
    LOG_ERR(TAG, "AP configuration rejected");
    return false;
  }
  if (isHeld() || foreignRadioActive()) {
    LOG_ERR(TAG, "%s denied AP: radio busy", owner);
    return false;
  }
  // softAP enables AP mode itself. Never downgrade a rejected password to open.
  if (!WiFi.softAP(ssid, password, channel, false, maxClients)) {
    LOG_ERR(TAG, "%s could not start AP", owner);
    stopWifi();
    return false;
  }
  mode_ = Mode::WifiAccessPoint;
  owner_ = owner;
  acquiredAtMs_ = static_cast<uint32_t>(millis());
  channel_ = channel;
  LOG_INF(TAG, "%s acquired AP", owner);
  return true;
}
bool RadioManager::accessPointAddress(const char* owner, uint8_t (&address)[4]) const {
  memset(address, 0, sizeof(address));
  if (!owner || owner_ != owner || mode_ != Mode::WifiAccessPoint) return false;
  const IPAddress ip = WiFi.softAPIP();
  if (ip == IPAddress(0, 0, 0, 0)) return false;
  for (size_t i = 0; i < sizeof(address); ++i) address[i] = ip[i];
  return true;
}

bool RadioManager::acquire(const Mode mode, const char* owner) {
  if (mode == Mode::Off || mode == Mode::WifiAccessPoint) {
    LOG_ERR(TAG, "Use shutdown for Off or acquireAccessPoint for configured AP");
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
    case Mode::WifiAccessPoint:  // only the configured acquisition path may start AP
    case Mode::Off:
      return false;
  }
  return false;
}

bool RadioManager::configureEspNow(const char* owner, const uint8_t channel) {
  if (!owner || owner_ != owner || mode_ != Mode::EspNow || channel < kMinChannel || channel > kMaxChannel) {
    LOG_ERR(TAG, "ESP-NOW configuration requires its owner and a valid channel");
    return false;
  }
  if (!WiFi.setSleep(false) || esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) != ESP_OK ||
      esp_wifi_set_ps(WIFI_PS_NONE) != ESP_OK) {
    LOG_ERR(TAG, "Could not configure ESP-NOW radio");
    return false;
  }
  channel_ = channel;
  return true;
}

void RadioManager::stopWifi() {
  sPickerLoggingActive = false;
  // Take it all the way down. Leaving the driver in STA-idle is what makes the
  // *next* screen's radio behave unpredictably.
  esp_wifi_set_promiscuous(false);
  if (WiFi.getMode() & WIFI_MODE_AP) WiFi.softAPdisconnect(true);
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_MODE_NULL);
}

bool RadioManager::stationMac(uint8_t (&mac)[6]) {
  memset(mac, 0, sizeof(mac));
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) return true;
  LOG_ERR(TAG, "Failed to read station MAC");
  return false;
}

bool RadioManager::pickerStatus(const char* owner, PickerStatus& out) const {
  out = {};
  if (!pickerAccessAllowed(owner)) return false;
  const auto status = WiFi.status();
  out.code = static_cast<int>(status);
  out.name = wifiStatusName(status);
  out.connected = status == WL_CONNECTED;
  out.failed = status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL || status == WL_CONNECTION_LOST;
  out.networkNotFound = status == WL_NO_SSID_AVAIL;
  if (out.connected) {
    out.rssi = WiFi.RSSI();
    const IPAddress ip = WiFi.localIP();
    for (size_t i = 0; i < sizeof(out.ip); ++i) out.ip[i] = ip[i];
    WiFi.BSSID(out.bssid);
    out.channel = WiFi.channel();
  }
  return true;
}

void RadioManager::setPickerEventLogging(const char* owner, const bool active, const bool resetReason) {
  if (!pickerAccessAllowed(owner)) return;
  if (resetReason) sPickerDisconnectReason = 0;
  if (active && !sPickerEventsRegistered) {
    sPickerEventsRegistered = WiFi.onEvent(logWifiStationEvent) != 0;
    if (!sPickerEventsRegistered) LOG_ERR(TAG, "Could not register picker event logging");
  }
  sPickerLoggingActive = active && sPickerEventsRegistered;
}

void RadioManager::logPickerDisconnectReason(const char* owner) const {
  if (!pickerAccessAllowed(owner)) return;
  const uint8_t reason = sPickerDisconnectReason.load();
  if (reason)
    LOG_INF(TAG, "Last disconnect reason: %u(%s)", reason,
            WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(reason)));
}

bool RadioManager::resolveHostname(const char* owner, const char* hostname, char (&address)[48]) {
  address[0] = 0;
  if (!stationConnected(owner) || !hostname || !*hostname || strnlen(hostname, 254) > 253) {
    LOG_ERR(TAG, "DNS requires an owned station and bounded hostname");
    return false;
  }
  for (const char* c = hostname; *c; ++c) {
    if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '-' || *c == '.')) {
      LOG_ERR(TAG, "DNS hostname contains unsupported characters");
      return false;
    }
  }
  IPAddress ip;
  if (WiFi.hostByName(hostname, ip) != 1 || !stationConnected(owner)) {
    LOG_ERR(TAG, "DNS lookup failed or station disconnected");
    return false;
  }
  // SDK formats IPv4/IPv6 using one bounded temporary String per submitted lookup.
  const String formatted = ip.toString();
  if (formatted.length() >= sizeof(address)) {
    LOG_ERR(TAG, "DNS address too long");
    return false;
  }
  snprintf(address, sizeof(address), "%s", formatted.c_str());
  return true;
}

int RadioManager::browseMdns(const char* owner, const char* service, MdnsResult* out, size_t capacity) {
  if (!stationConnected(owner) || !out || !capacity || !service || service[0] != '_' || strnlen(service, 17) < 2 ||
      strnlen(service, 17) > 16) {
    LOG_ERR(TAG, "mDNS requires owned station, service and result buffer");
    return -1;
  }
  for (const char* c = service + 1; *c; ++c) {
    if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '-')) {
      LOG_ERR(TAG, "Invalid mDNS service type");
      return -1;
    }
  }
  // Do not take over another subsystem's responder, even if its radio is stale.
  char existing[MDNS_NAME_BUF_LEN] = {};
  if (mdns_hostname_get(existing) != ESP_ERR_INVALID_STATE) {
    LOG_ERR(TAG, "mDNS responder already active");
    return -1;
  }
  if (mdns_init() != ESP_OK) {
    LOG_ERR(TAG, "mDNS startup failed");
    return -1;
  }
  capacity = capacity < kMaxMdnsResults ? capacity : kMaxMdnsResults;
  mdns_result_t* results = nullptr;
  // SDK owns transient query/task allocations; max_results bounds records, not
  // TXT/address bytes. Copy only fixed fields and free every result before return.
  const esp_err_t error = mdns_query_ptr(service, "_tcp", 2000, capacity, &results);
  int count = 0;
  if (error == ESP_OK && stationConnected(owner)) {
    for (const mdns_result_t* r = results; r && static_cast<size_t>(count) < capacity; r = r->next) {
      MdnsResult& item = out[count++];
      item = {};
      auto copyName = [](char* target, size_t size, const char* source) {
        snprintf(target, size, "%s", source ? source : "");
        for (char* c = target; *c; ++c)
          if (static_cast<unsigned char>(*c) < 32 || *c == 127) *c = '?';
      };
      copyName(item.instance, sizeof(item.instance), r->instance_name);
      copyName(item.hostname, sizeof(item.hostname), r->hostname);
      item.port = r->port;
      for (const mdns_ip_addr_t* ip = r->addr; ip; ip = ip->next) {
        if (ip->addr.type == ESP_IPADDR_TYPE_V4 && !item.ipv4[0]) {
          snprintf(item.ipv4, sizeof(item.ipv4), IPSTR, IP2STR(&ip->addr.u_addr.ip4));
        } else if (ip->addr.type == ESP_IPADDR_TYPE_V6 && !item.ipv6[0]) {
          ip6_addr_t address = {};
          memcpy(address.addr, ip->addr.u_addr.ip6.addr, sizeof(address.addr));
          char formatted[40];
          if (ip6addr_ntoa_r(&address, formatted, sizeof(formatted))) {
            const unsigned zone = ip->addr.u_addr.ip6.zone;
            if (zone)
              snprintf(item.ipv6, sizeof(item.ipv6), "%s%%%u", formatted, zone);
            else
              snprintf(item.ipv6, sizeof(item.ipv6), "%s", formatted);
          } else {
            LOG_ERR(TAG, "mDNS IPv6 formatting failed");
          }
        }
        if (item.ipv4[0] && item.ipv6[0]) break;
      }
    }
  } else {
    LOG_ERR(TAG, "mDNS query failed (%d) or station disconnected", static_cast<int>(error));
    count = -1;
  }
  mdns_query_results_free(results);
  mdns_free();
  return count;
}

bool RadioManager::preparePickerConnection(const char* owner) {
  if (!pickerAccessAllowed(owner)) return false;
  setPickerEventLogging(owner, false, true);
  // Credentials belong to WifiCredentialStore, never the SDK's persistent store.
  WiFi.persistent(false);
  if (!WiFi.mode(WIFI_STA)) {
    LOG_ERR(TAG, "Could not start picker station");
    return false;
  }
  // Preserve the existing non-destructive disconnect and timeout before begin.
  if (!WiFi.disconnect(false, false, 1000)) {
    LOG_DBG(TAG, "Disconnect before begin timed out; continuing with explicit begin");
  }
  delay(100);
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  uint8_t mac[6] = {};
  char hostname[40];
  if (WiFi.macAddress(mac)) {
    snprintf(hostname, sizeof(hostname), "CrossPoint-Reader-%02X%02X%02X%02X%02X%02X", unsigned(mac[0]),
             unsigned(mac[1]), unsigned(mac[2]), unsigned(mac[3]), unsigned(mac[4]), unsigned(mac[5]));
    if (!WiFi.setHostname(hostname)) LOG_ERR(TAG, "Could not set picker hostname; retaining SDK hostname");
  } else {
    LOG_ERR(TAG, "Could not read station MAC; retaining SDK hostname");
  }
  return true;
}

int RadioManager::beginPickerConnection(const char* owner, const char* ssid, const char* password) {
  if (!pickerAccessAllowed(owner)) return WL_CONNECT_FAILED;
  if (!ssid || strnlen(ssid, 33) == 0 || strnlen(ssid, 33) > 32 || (password && strnlen(password, 65) > 64)) {
    LOG_ERR(TAG, "Invalid picker credential length");
    return WL_CONNECT_FAILED;
  }
  setPickerEventLogging(owner, true, true);
  return password ? WiFi.begin(ssid, password) : WiFi.begin(ssid);
}

void RadioManager::disconnectPicker(const char* owner, const bool finish) {
  if (!pickerAccessAllowed(owner)) return;
  if (!WiFi.disconnect(false)) LOG_DBG(TAG, "Picker disconnect did not complete");
  if (finish) {
    delay(30);
  }
}

int RadioManager::startPickerScan(const char* owner) {
  static_assert(WIFI_SCAN_RUNNING == kScanRunning && WIFI_SCAN_FAILED == kScanFailed);
  if (!pickerAccessAllowed(owner)) return kScanFailed;
  WiFi.disconnect();
  delay(100);
  const int result = WiFi.scanNetworks(true);
  if (result == kScanFailed) LOG_ERR(TAG, "Could not start picker scan");
  return result;
}

int RadioManager::pickerScanCount(const char* owner) {
  if (!pickerAccessAllowed(owner)) return kScanFailed;
  const int count = WiFi.scanComplete();
  if (count < 0) return count;
  return count > static_cast<int>(kMaxScanResults) ? static_cast<int>(kMaxScanResults) : count;
}

bool RadioManager::pickerScanResult(const char* owner, const size_t index, ScanResult& out) {
  out = {};
  const int count = pickerScanCount(owner);
  if (count < 0 || index >= static_cast<size_t>(count)) {
    LOG_ERR(TAG, "Picker scan result unavailable");
    return false;
  }
  const int i = static_cast<int>(index);
  const String ssid = WiFi.SSID(i);
  snprintf(out.ssid, sizeof(out.ssid), "%s", ssid.c_str());
  const uint8_t* bssid = WiFi.BSSID(i);
  if (bssid) memcpy(out.bssid, bssid, sizeof(out.bssid));
  out.rssi = static_cast<int8_t>(WiFi.RSSI(i));
  out.channel = static_cast<uint8_t>(WiFi.channel(i));
  out.encrypted = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  return true;
}

void RadioManager::clearPickerScan(const char* owner) {
  if (pickerAccessAllowed(owner)) WiFi.scanDelete();
}

int RadioManager::scanNetworks(ScanResult* out, const size_t capacity, const bool passive) {
  if (out == nullptr || capacity == 0) return -1;
  if (mode_ != Mode::WifiScan && mode_ != Mode::WifiStation) {
    LOG_ERR(TAG, "scanNetworks needs a WifiScan hold (mode is %s)", modeName(mode_));
    return -1;
  }

  const int found = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/true, passive);
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
