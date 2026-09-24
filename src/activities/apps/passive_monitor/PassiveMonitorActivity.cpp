#include "PassiveMonitorActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <RadioManager.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"
namespace {
constexpr char kPacketOwner[] = "packet_monitor", kProbeOwner[] = "probe_sniffer", kDeauthOwner[] = "deauth_detector";
void put32(uint8_t* out, uint32_t value) {
  for (int i = 0; i < 4; ++i) out[i] = uint8_t(value >> (i * 8));
}
}  // namespace
const char* PassiveMonitorActivity::owner() const {
  return kind == Kind::Packets ? kPacketOwner : kind == Kind::Probes ? kProbeOwner : kDeauthOwner;
}
void PassiveMonitorActivity::receive(void* context, const uint8_t* bytes, uint16_t length, int8_t rssi,
                                     uint8_t channel) {
  if (!context || !bytes || length <= 4) return;
  // Native C3/S3 RX sig_len includes four FCS bytes. Copy only; parse in loop().
  Packet packet;
  packet.original = length - 4;
  packet.rssi = rssi;
  packet.channel = channel;
  const uint16_t copied = std::min<uint16_t>(packet.original, sizeof(packet.bytes));
  memcpy(packet.bytes, bytes, copied);
  static_cast<PassiveMonitorActivity*>(context)->packets.push(reinterpret_cast<const uint8_t*>(&packet), 4 + copied);
}
void PassiveMonitorActivity::onEnter() {
  Activity::onEnter();
  owned = RADIO.acquire(RadioManager::Mode::WifiPromiscuous, owner());
  running = owned && RADIO.startPromiscuous(owner(), receive, this, channel);
  failed = !running;
  intervalStart = lastRefresh = lastHop = millis();
  requestUpdate();
}
void PassiveMonitorActivity::onExit() {
  Activity::onExit();
  if (owned) {
    RADIO.stopPromiscuous(owner());
    running = false;
    closeCapture(2);
    if (RADIO.shutdown(owner())) owned = false;
  }
}
void PassiveMonitorActivity::process(const Packet& packet) {
  const uint16_t n = std::min<uint16_t>(packet.original, sizeof(packet.bytes));
  if (n < 2) return;
  ++total;
  if (packet.channel >= 1 && packet.channel <= 13) ++channelFrames[packet.channel];
  const uint8_t type = (packet.bytes[0] >> 2) & 3;
  const uint8_t subtype = packet.bytes[0] >> 4;
  if (type == 0)
    ++management;
  else if (type == 1)
    ++control;
  else if (type == 2)
    ++dataFrames;
  if (kind == Kind::Packets && (type == 0 || type == 2) && n >= 16)
    track(packet.bytes + 10, nullptr, packet.rssi, packet.channel);
  if (type != 0 || n < 24) return;
  if (subtype == 4)
    ++probes;
  else if (subtype == 12) {
    ++deauth;
    ++intervalCount;
  } else if (subtype == 10)
    ++disassoc;
  const bool wanted = kind == Kind::Probes ? subtype == 4 : kind == Kind::Deauth && (subtype == 12 || subtype == 10);
  if (!wanted) return;
  Event& event = events[eventHead];
  event = {};
  memcpy(event.destination, packet.bytes + 4, 6);
  memcpy(event.source, packet.bytes + 10, 6);
  event.rssi = packet.rssi;
  event.channel = packet.channel;
  event.subtype = subtype;
  event.protectedFrame = (packet.bytes[1] & 0x40) != 0;
  if (!event.protectedFrame && kind == Kind::Deauth && n >= 26) {
    event.reason = uint16_t(packet.bytes[24]) | (uint16_t(packet.bytes[25]) << 8);
    event.reasonKnown = true;
  }
  if (kind == Kind::Probes && !event.protectedFrame) {
    for (uint16_t at = 24; at + 2 <= n;) {
      const uint8_t id = packet.bytes[at], size = packet.bytes[at + 1];
      at += 2;
      if (at + size > n) break;
      if (id == 0) {
        if (size <= 32) {
          for (uint8_t i = 0; i < size; ++i) {
            const uint8_t c = packet.bytes[at + i];
            event.ssid[i] = c >= 32 && c < 127 ? char(c) : '?';
          }
        }
        break;
      }
      at += size;
    }
  }
  if (kind == Kind::Probes) track(event.source, event.ssid, event.rssi, event.channel);
  eventHead = (eventHead + 1) % 8;
  if (eventCount < 8) ++eventCount;
}
void PassiveMonitorActivity::track(const uint8_t* mac, const char* ssid, int8_t rssi, uint8_t seenChannel) {
  // Source MACs may be randomized; do not interpret the table as a device count.
  if (mac[0] & 1) return;
  uint8_t i = 0;
  for (; i < peerCount; ++i)
    if (memcmp(peers[i].mac, mac, 6) == 0) break;
  if (i == peerCount) {
    if (peerCount == 24) {
      ++untracked;
      return;
    }
    memcpy(peers[i].mac, mac, 6);
    ++peerCount;
  }
  Peer& peer = peers[i];
  if (peer.frames != UINT32_MAX) ++peer.frames;
  peer.rssi = rssi;
  peer.channel = seenChannel;
  if (ssid) snprintf(peer.ssid, sizeof(peer.ssid), "%s", ssid);
}
void PassiveMonitorActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  RenderLock lock(*this);
  if (csvStatus && mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    csvStatus = 0;
    requestUpdate();
  } else if (owned && mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (running) {
      RADIO.stopPromiscuous(owner());
      running = false;
    } else {
      running = RADIO.startPromiscuous(owner(), receive, this, channel);
      failed = !running;
      intervalStart = millis();
      intervalCount = 0;
    }
    requestUpdate();
  }
  if (owned && mappedInput.wasPressed(MappedInputManager::Button::PageForward)) {
    hopping = !hopping;
    lastHop = millis();
    requestUpdate();
  }
  const bool right = mappedInput.wasPressed(MappedInputManager::Button::Right);
  const bool left = mappedInput.wasPressed(MappedInputManager::Button::Left);
  const bool hop = running && hopping && millis() - lastHop >= 1000;
  if (owned && (right || left || hop)) {
    const uint8_t next = uint8_t((channel + (left ? 11 : 0)) % 13 + 1);
    if (!running || RADIO.setChannel(owner(), next))
      channel = next;
    else {
      RADIO.stopPromiscuous(owner());
      running = false;
      failed = true;
    }
    lastHop = millis();
    requestUpdate();
  }
  const bool up = mappedInput.wasPressed(MappedInputManager::Button::Up);
  const bool down = mappedInput.wasPressed(MappedInputManager::Button::Down);
  const uint8_t rows = kind == Kind::Probes ? peerCount : eventCount;
  if (kind == Kind::Packets && up) {
    chart = !chart;
    requestUpdate();
  } else if (rows && (up || down)) {
    selected = (selected + (down ? 1 : rows - 1)) % rows;
    requestUpdate();
  }
  if (owned && ((kind == Kind::Packets && down) ||
                (kind == Kind::Probes && mappedInput.wasPressed(MappedInputManager::Button::PageBack)))) {
    csvStatus = saveCsv() ? 1 : -1;
    requestUpdate();
  }
  if (owned && mappedInput.wasPressed(MappedInputManager::Button::PageBack)) {
    if (kind == Kind::Packets) toggleCapture();
    if (kind == Kind::Deauth) spike = false;
    requestUpdate();
  }
  // Bound work per loop so a busy channel cannot starve input/exit handling.
  for (int i = 0; i < 32; ++i) {
    const uint16_t n = packets.pop(reinterpret_cast<uint8_t*>(&scratch), sizeof(scratch));
    if (!n) break;
    if (n < 4 || n != 4 + std::min<uint16_t>(scratch.original, sizeof(scratch.bytes))) continue;
    process(scratch);
    if (captureStatus == 1) writeCapture(scratch);
  }
  if (running && kind == Kind::Deauth && millis() - intervalStart >= 2000) {
    const uint32_t now = millis();
    if (intervalCount >= 5) {
      spike = true;
      spikeFrames = intervalCount;
      spikeElapsed = now - intervalStart;
    }
    intervalCount = 0;
    intervalStart = now;
  }
  if (running && millis() - lastRefresh >= 1000) {
    lastRefresh = millis();
    requestUpdate();
  }
}
void PassiveMonitorActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const char* title = kind == Kind::Packets  ? tr(STR_APP_PACKET_MONITOR)
                      : kind == Kind::Probes ? tr(STR_APP_PROBE_SNIFFER)
                                             : tr(STR_APP_DEAUTH_DETECTOR);
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::draw(renderer, header, title, false);
  else
    GUI.drawHeader(renderer, header, title);
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 6;
  int y = screen.y + line;
  char text[112];
  auto draw = [&](const char* s) {
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, s);
    y += line;
  };
  if (failed) draw(tr(STR_RADIO_BUSY_OR_UNAVAILABLE));
  snprintf(text, sizeof(text), tr(STR_MONITOR_STATE), unsigned(channel),
           running ? tr(STR_MONITOR_LIVE) : tr(STR_MONITOR_PAUSED),
           hopping ? tr(STR_MONITOR_HOPPING) : tr(STR_MONITOR_FIXED));
  draw(text);
  snprintf(text, sizeof(text), tr(STR_MONITOR_COUNTS), static_cast<unsigned long>(total),
           static_cast<unsigned long>(packets.droppedFrames()));
  draw(text);
  if (csvStatus) draw(csvStatus > 0 ? csvPath : tr(STR_MDNS_EXPORT_FAILED));
  if (kind == Kind::Packets && chart && !csvStatus) {
    renderChannels(y);
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_MONITOR_PAUSE), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }
  if (kind == Kind::Packets) {
    snprintf(text, sizeof(text), tr(STR_MONITOR_TRACKED), unsigned(peerCount), static_cast<unsigned long>(untracked));
    draw(text);
    snprintf(text, sizeof(text), tr(STR_MONITOR_TYPES), static_cast<unsigned long>(management),
             static_cast<unsigned long>(dataFrames), static_cast<unsigned long>(control));
    draw(text);
    draw(captureStatus == 1   ? tr(STR_PCAP_RECORDING)
         : captureStatus == 2 ? tr(STR_PCAP_SAVED)
         : captureStatus == 3 ? tr(STR_PCAP_FULL)
         : captureStatus < 0  ? tr(STR_PCAP_FAILED)
                              : tr(STR_PCAP_HINT));
    if (capturePath[0]) draw(capturePath);
    draw(tr(STR_PCAP_LIMITS));
  } else {
    if (kind == Kind::Probes)
      snprintf(text, sizeof(text), tr(STR_MONITOR_PROBES), static_cast<unsigned long>(probes));
    else
      snprintf(text, sizeof(text), tr(STR_MONITOR_DEAUTH), static_cast<unsigned long>(deauth),
               static_cast<unsigned long>(disassoc));
    draw(text);
    if (kind == Kind::Probes) {
      snprintf(text, sizeof(text), tr(STR_MONITOR_TRACKED), unsigned(peerCount), static_cast<unsigned long>(untracked));
      draw(text);
      if (peerCount) {
        const Peer& peer = peers[selected % peerCount];
        snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X", peer.mac[0], peer.mac[1], peer.mac[2],
                 peer.mac[3], peer.mac[4], peer.mac[5]);
        draw(text);
        draw(peer.ssid[0] ? peer.ssid : tr(STR_MONITOR_SSID_UNKNOWN));
        snprintf(text, sizeof(text), tr(STR_PROBE_SUMMARY), static_cast<unsigned long>(peer.frames), int(peer.rssi),
                 unsigned(peer.channel));
        draw(text);
      }
    } else if (eventCount) {
      const Event& event = events[(eventHead + 7 - selected) % 8];
      snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X", event.source[0], event.source[1], event.source[2],
               event.source[3], event.source[4], event.source[5]);
      draw(text);
      snprintf(text, sizeof(text), tr(STR_MONITOR_EVENT), unsigned(event.channel), int(event.rssi),
               unsigned(event.subtype));
      draw(text);
      if (kind == Kind::Probes)
        draw(event.ssid[0] ? event.ssid : tr(STR_MONITOR_SSID_UNKNOWN));
      else if (event.protectedFrame)
        draw(tr(STR_MONITOR_PROTECTED));
      else if (event.reasonKnown) {
        snprintf(text, sizeof(text), tr(STR_MONITOR_REASON), unsigned(event.reason));
        draw(text);
      } else
        draw(tr(STR_MONITOR_REASON_UNKNOWN));
    }
    if (kind == Kind::Deauth && spike) {
      snprintf(text, sizeof(text), tr(STR_DEAUTH_SPIKE), static_cast<unsigned long>(spikeFrames),
               static_cast<unsigned long>(spikeElapsed));
      draw(text);
    }
    draw(tr(STR_MONITOR_OBSERVATION));
  }
  draw(kind == Kind::Packets  ? tr(STR_PACKET_MORE_CONTROLS)
       : kind == Kind::Probes ? tr(STR_PROBE_MORE_CONTROLS)
                              : tr(STR_DEAUTH_MORE_CONTROLS));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), running ? tr(STR_MONITOR_PAUSE) : tr(STR_MONITOR_RESUME),
                                            tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
void PassiveMonitorActivity::closeCapture(int status) {
  if (!capture) return;
  const bool synced = capture.sync();
  const bool closed = capture.close();
  captureStatus = synced && closed ? status : -1;
  if (captureStatus < 0) LOG_ERR("MON", "Capture sync/close failed; file may be partial");
}
void PassiveMonitorActivity::toggleCapture() {
  if (captureStatus == 1) {
    closeCapture(2);
    return;
  }
  constexpr char dir[] = "/crossink/packets";
  if (!Storage.exists(dir) && !Storage.mkdir(dir, true)) {
    LOG_ERR("MON", "Capture mkdir failed");
    captureStatus = -1;
    return;
  }
  for (int i = 0; i < 100; ++i) {
    snprintf(capturePath, sizeof(capturePath), "%s/capture-%02d.pcap", dir, i);
    if (Storage.exists(capturePath)) continue;
    capture = Storage.open(capturePath, O_WRITE | O_CREAT | O_EXCL);
    break;
  }
  if (!capture) {
    LOG_ERR("MON", "Capture slots exhausted or open failed");
    captureStatus = -1;
    return;
  }
  // Classic little-endian PCAP, raw IEEE 802.11 without FCS, 96-byte snap length.
  uint8_t header[24] = {};
  put32(header, 0xa1b2c3d4);
  header[4] = 2;
  header[6] = 4;
  put32(header + 16, 96);
  put32(header + 20, 105);
  if (capture.write(header, sizeof(header)) != sizeof(header)) {
    LOG_ERR("MON", "Capture header write failed");
    closeCapture(-1);
    return;
  }
  captureBytes = syncedBytes = sizeof(header);
  captureStatus = 1;
}
void PassiveMonitorActivity::writeCapture(const Packet& packet) {
  const uint32_t bytes = std::min<uint16_t>(packet.original, sizeof(packet.bytes));
  if (captureBytes + 16 + bytes > 1024 * 1024) {
    closeCapture(3);
    return;
  }
  // Dequeue-time uptime, not wall-clock or a claimed hardware arrival timestamp.
  const uint32_t now = millis();
  uint8_t header[16];
  put32(header, now / 1000);
  put32(header + 4, (now % 1000) * 1000);
  put32(header + 8, bytes);
  put32(header + 12, packet.original);
  if (capture.write(header, sizeof(header)) != sizeof(header) || capture.write(packet.bytes, bytes) != bytes) {
    LOG_ERR("MON", "Capture write failed; file may be partial");
    closeCapture(-1);
    return;
  }
  captureBytes += 16 + bytes;
  if (captureBytes - syncedBytes >= 4096) {
    if (!capture.sync()) {
      LOG_ERR("MON", "Capture sync failed");
      closeCapture(-1);
      return;
    }
    syncedBytes = captureBytes;
  }
}

void PassiveMonitorActivity::renderChannels(int top) {
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 6;
  const int bottom = screen.y + screen.height - 3 * line;
  const int height = bottom - top;
  const int width = (screen.width - 24) / 13;
  if (height <= 0 || width < 3) return;
  uint32_t maximum = 1;
  for (int c = 1; c <= 13; ++c) maximum = std::max(maximum, channelFrames[c]);
  char text[8];
  for (int c = 1; c <= 13; ++c) {
    const int x = screen.x + 12 + (c - 1) * width;
    const int bar = int(uint64_t(channelFrames[c]) * height / maximum);
    if (bar) renderer.fillRect(x + 1, bottom - bar, width - 2, bar, true);
    snprintf(text, sizeof(text), "%d", c);
    renderer.drawText(UI_10_FONT_ID, x, bottom + 2, text);
  }
  UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, bottom + line, tr(STR_PACKET_CHANNEL_NOTE));
}
bool PassiveMonitorActivity::saveCsv() {
  constexpr char dir[] = "/crossink/monitor";
  if (!Storage.exists(dir) && !Storage.mkdir(dir, true)) {
    LOG_ERR("MON", "CSV mkdir failed");
    return false;
  }
  HalFile file;
  for (int slot = 0; slot < 100; ++slot) {
    snprintf(csvPath, sizeof(csvPath), "%s/%s-%02d.csv", dir, kind == Kind::Probes ? "probes" : "channels", slot);
    if (Storage.exists(csvPath)) continue;
    file = Storage.open(csvPath, O_WRITE | O_CREAT | O_EXCL);
    break;
  }
  if (!file) {
    LOG_ERR("MON", "CSV open failed or slots exhausted");
    return false;
  }
  auto write = [&file](const char* value) {
    const size_t n = strlen(value);
    return file.write(value, n) == n;
  };
  char row[96];
  bool ok = write(kind == Kind::Probes ? "source_mac,ssid,last_rssi_dbm,channel,frames\n" : "metric,key,count\n");
  if (kind == Kind::Packets) {
    for (int c = 1; ok && c <= 13; ++c) {
      snprintf(row, sizeof(row), "channel,%d,%lu\n", c, static_cast<unsigned long>(channelFrames[c]));
      ok = write(row);
    }
    snprintf(row, sizeof(row), "queue_drops,,%lu\nuntracked_source_frames,,%lu\n",
             static_cast<unsigned long>(packets.droppedFrames()), static_cast<unsigned long>(untracked));
    ok = ok && write(row);
  }
  for (int i = 0; ok && i < peerCount; ++i) {
    const Peer& peer = peers[i];
    snprintf(row, sizeof(row), "%s%02X:%02X:%02X:%02X:%02X:%02X,", kind == Kind::Packets ? "transmitter," : "",
             peer.mac[0], peer.mac[1], peer.mac[2], peer.mac[3], peer.mac[4], peer.mac[5]);
    ok = write(row);
    if (kind == Kind::Probes) {
      char quoted[68];
      size_t n = 0;
      quoted[n++] = '"';
      const char* first = peer.ssid;
      while (*first == ' ') ++first;
      if (*first && strchr("=+-@", *first)) quoted[n++] = '\'';
      for (const char* c = peer.ssid; *c; ++c) {
        if (*c == '"') quoted[n++] = '"';
        quoted[n++] = *c;
      }
      quoted[n++] = '"';
      quoted[n] = 0;
      ok = ok && write(quoted);
      snprintf(row, sizeof(row), ",%d,%u,%lu\n", int(peer.rssi), unsigned(peer.channel),
               static_cast<unsigned long>(peer.frames));
    } else
      snprintf(row, sizeof(row), "%lu\n", static_cast<unsigned long>(peer.frames));
    ok = ok && write(row);
  }
  if (ok) ok = file.sync();
  const bool closed = file.close();
  if (!ok || !closed) {
    LOG_ERR("MON", "CSV write/sync/close failed");
    if (!Storage.remove(csvPath)) LOG_ERR("MON", "Partial CSV cleanup failed");
    return false;
  }
  return true;
}
