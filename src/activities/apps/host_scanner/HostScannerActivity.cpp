#include "HostScannerActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <RadioManager.h>

#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"
namespace {
constexpr char kOwner[] = "host_scanner";
constexpr uint16_t kPorts[] = {21, 22, 23, 25, 53, 80, 139, 443, 445, 993, 3389, 5900, 8080, 8443};
constexpr int kPortCount = sizeof(kPorts) / sizeof(kPorts[0]);
void formatIp(uint32_t ip, char (&out)[16]) {
  snprintf(out, sizeof(out), "%u.%u.%u.%u", unsigned(ip >> 24), unsigned((ip >> 16) & 255), unsigned((ip >> 8) & 255),
           unsigned(ip & 255));
}
}  // namespace
void HostScannerActivity::onEnter() {
  Activity::onEnter();
  owned = RADIO.acquire(RadioManager::Mode::WifiStation, kOwner);
  if (!owned) {
    state = State::Failed;
    requestUpdate();
    return;
  }
  auto picker = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput, true, false, kOwner);
  if (!picker) {
    LOG_ERR("HOST", "Picker allocation failed");
    state = State::Failed;
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(picker), [this](const ActivityResult& result) {
    if (result.isCancelled) {
      finish();
      return;
    }
    state = RADIO.stationIpv4Range(kOwner, first, last, self) ? State::Ready : State::Failed;
    requestUpdate();
  });
}
void HostScannerActivity::onExit() {
  Activity::onExit();
  if (owned && RADIO.shutdown(kOwner)) owned = false;
}
void HostScannerActivity::startScan() {
  uint32_t begin = 0, end = 0, own = 0;
  const bool ok = RADIO.stationIpv4Range(kOwner, begin, end, own);
  RenderLock lock(*this);
  first = begin;
  last = end;
  self = own;
  next = begin;
  count = selected = 0;
  partial = false;
  state = ok ? State::Scanning : State::Failed;
  requestUpdate();
}
void HostScannerActivity::step() {
  // Detect DHCP/subnet changes before every connection; never continue an old range.
  uint32_t begin = 0, end = 0, own = 0;
  if (!RADIO.stationIpv4Range(kOwner, begin, end, own) || begin != first || end != last || own != self) {
    LOG_ERR("HOST", "Station/subnet changed during scan");
    RenderLock lock(*this);
    partial = true;
    state = State::Failed;
    requestUpdate();
    return;
  }
  const bool scanning = state == State::Scanning;
  if (scanning && next == self) {
    RenderLock lock(*this);
    ++next;
  }
  if (scanning && next > last) {
    RenderLock lock(*this);
    state = State::Hosts;
    requestUpdate();
    return;
  }
  char ip[16];
  formatIp(scanning ? next : hosts[selected].ip, ip);
  uint32_t elapsed = 0;
  const int result = RADIO.probeTcp(kOwner, ip, scanning ? 80 : kPorts[portIndex], scanning ? 200 : 500, elapsed);
  RenderLock lock(*this);
  if (result < 0) {
    partial = true;
    state = State::Failed;
    requestUpdate();
    return;
  }
  if (scanning) {
    if (result == 1) hosts[count++] = {next, uint16_t(1u << 5), uint16_t(1u << 5)};
    ++next;
    if (count == 32 || next > last) {
      partial = next <= last;
      state = State::Hosts;
    }
    if (result == 1 || state == State::Hosts || (next - first) % 16 == 0) requestUpdate();
  } else {
    hosts[selected].tested |= uint16_t(1u << portIndex);
    if (result == 1) hosts[selected].open |= uint16_t(1u << portIndex);
    if (++portIndex == kPortCount) state = State::Hosts;
    requestUpdate();
  }
}
void HostScannerActivity::loop() {
  const bool back = TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
                    mappedInput.wasPressed(MappedInputManager::Button::Back);
  if (exportStatus) {
    if (back || mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      RenderLock lock(*this);
      exportStatus = 0;
      requestUpdate();
    }
    return;
  }
  if (back) {
    if (state == State::Scanning || state == State::Ports) {
      RenderLock lock(*this);
      partial = true;
      state = State::Hosts;
      requestUpdate();
    } else
      finish();
    return;
  }
  if (state == State::Scanning || state == State::Ports) {
    step();
    return;
  }
  if (state == State::Hosts && count) {
    if (mappedInput.wasPressed(MappedInputManager::Button::PageBack)) {
      int slot = -1;
      const bool ok = saveCsv(slot);
      RenderLock lock(*this);
      exportStatus = ok ? 1 : -1;
      exportSlot = slot;
      requestUpdate();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Left) ||
        mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      RenderLock lock(*this);
      selected = (selected + (mappedInput.wasPressed(MappedInputManager::Button::Right) ? 1 : count - 1)) % count;
      requestUpdate();
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      RenderLock lock(*this);
      portIndex = 0;
      hosts[selected].open = hosts[selected].tested = 0;
      state = State::Ports;
      requestUpdate();
      return;
    }
  }
  if (owned && (((state == State::Ready || state == State::Failed || (state == State::Hosts && !count)) &&
                 mappedInput.wasPressed(MappedInputManager::Button::Confirm)) ||
                (state == State::Hosts && mappedInput.wasPressed(MappedInputManager::Button::PageForward))))
    startScan();
}
void HostScannerActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_HOST_SCANNER), false);
  else
    GUI.drawHeader(renderer, header, tr(STR_APP_HOST_SCANNER));
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  int y = screen.y + line;
  char text[112], ip[16], end[16];
  auto draw = [&](const char* value) {
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, value);
    y += line;
  };
  draw(tr(STR_HOST_SCOPE));
  if (exportStatus) {
    if (exportStatus > 0)
      snprintf(text, sizeof(text), tr(STR_HOST_SAVED), exportSlot);
    else
      snprintf(text, sizeof(text), "%s", tr(STR_MDNS_EXPORT_FAILED));
    draw(text);
  } else if (state == State::Failed)
    draw(tr(STR_TCP_FAILED));
  else if (state == State::Waiting)
    draw(tr(STR_LOADING));
  else if (state == State::Ready || state == State::Scanning) {
    formatIp(first, ip);
    formatIp(last, end);
    snprintf(text, sizeof(text), "%s - %s", ip, end);
    draw(text);
    if (state == State::Scanning) {
      snprintf(text, sizeof(text), tr(STR_HOST_PROGRESS), unsigned(next - first), unsigned(last - first + 1), count);
      draw(text);
    } else
      draw(tr(STR_HOST_START));
  } else {
    if (partial) draw(tr(STR_HOST_PARTIAL));
    if (!count)
      draw(tr(STR_HOST_EMPTY));
    else {
      const Host& host = hosts[selected];
      formatIp(host.ip, ip);
      snprintf(text, sizeof(text), "%d/%d  %s", selected + 1, count, ip);
      draw(text);
      int tested = 0;
      for (int i = 0; i < kPortCount; ++i)
        if (host.tested & (1u << i)) ++tested;
      snprintf(text, sizeof(text), tr(STR_HOST_TESTED), tested, kPortCount);
      draw(text);
      size_t used = 0;
      for (int i = 0; i < kPortCount; ++i)
        if (host.open & (1u << i))
          used += snprintf(text + used, sizeof(text) - used, "%s%u", used ? ", " : "", unsigned(kPorts[i]));
      if (!used) snprintf(text, sizeof(text), "%s", tr(STR_HOST_NONE_OPEN));
      draw(tr(STR_HOST_OPEN));
      draw(text);
      draw(state == State::Ports ? tr(STR_SCANNING) : tr(STR_HOST_ACTIONS));
    }
  }
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), owned ? tr(STR_MDNS_QUERY) : "", tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
bool HostScannerActivity::saveCsv(int& slot) const {
  constexpr char dir[] = "/crossink/hosts";
  if (!Storage.exists(dir) && !Storage.mkdir(dir, true)) {
    LOG_ERR("HOST", "Export mkdir failed");
    return false;
  }
  char path[40];
  HalFile file;
  for (slot = 0; slot < 100; ++slot) {
    snprintf(path, sizeof(path), "%s/scan-%02d.csv", dir, slot);
    if (Storage.exists(path)) continue;
    file = Storage.open(path, O_WRITE | O_CREAT | O_EXCL);
    break;
  }
  if (!file) {
    LOG_ERR("HOST", "Export open failed or slots exhausted");
    return false;
  }
  auto write = [&file](const char* s, size_t n) { return file.write(s, n) == n; };
  static constexpr char header[] = "ipv4,open_tcp_ports,tested_tcp_ports\n";
  bool ok = write(header, sizeof(header) - 1);
  for (int i = 0; ok && i < count; ++i) {
    char ip[16];
    formatIp(hosts[i].ip, ip);
    ok = write(ip, strlen(ip));
    for (int field = 0; ok && field < 2; ++field) {
      static constexpr char prefix[] = {',', '"'};
      ok = write(prefix, sizeof(prefix));
      bool firstPort = true;
      const uint16_t bits = field ? hosts[i].tested : hosts[i].open;
      for (int j = 0; ok && j < kPortCount; ++j)
        if (bits & (1u << j)) {
          char port[8];
          const int n = snprintf(port, sizeof(port), "%s%u", firstPort ? "" : ";", unsigned(kPorts[j]));
          ok = write(port, size_t(n));
          firstPort = false;
        }
      const char quote = '"';
      ok = ok && write(&quote, 1);
    }
    ok = ok && write("\n", 1);
  }
  if (ok) ok = file.sync();
  const bool closed = file.close();
  if (!ok || !closed) {
    LOG_ERR("HOST", "Export write/sync/close failed");
    if (!Storage.remove(path)) LOG_ERR("HOST", "Partial export removal failed");
    return false;
  }
  return true;
}
