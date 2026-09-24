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
  lastRefresh = lastHop = millis();
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
  const uint8_t type = (packet.bytes[0] >> 2) & 3;
  const uint8_t subtype = packet.bytes[0] >> 4;
  if (type == 0)
    ++management;
  else if (type == 1)
    ++control;
  else if (type == 2)
    ++dataFrames;
  if (type != 0 || n < 24) return;
  if (subtype == 4)
    ++probes;
  else if (subtype == 12)
    ++deauth;
  else if (subtype == 10)
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
  eventHead = (eventHead + 1) % 8;
  if (eventCount < 8) ++eventCount;
}
void PassiveMonitorActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  RenderLock lock(*this);
  if (owned && mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (running) {
      RADIO.stopPromiscuous(owner());
      running = false;
    } else {
      running = RADIO.startPromiscuous(owner(), receive, this, channel);
      failed = !running;
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
  if (eventCount && (mappedInput.wasPressed(MappedInputManager::Button::Up) ||
                     mappedInput.wasPressed(MappedInputManager::Button::Down))) {
    selected =
        (selected + (mappedInput.wasPressed(MappedInputManager::Button::Down) ? 1 : eventCount - 1)) % eventCount;
    requestUpdate();
  }
  if (kind == Kind::Packets && owned && mappedInput.wasPressed(MappedInputManager::Button::PageBack)) {
    toggleCapture();
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
  if (kind == Kind::Packets) {
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
    if (eventCount) {
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
    draw(tr(STR_MONITOR_OBSERVATION));
  }
  draw(tr(STR_MONITOR_CONTROLS));
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
