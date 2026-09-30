#include "TransitAlertActivity.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "activities/util/KeyboardEntryActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr char owner[] = "transit_alert";
constexpr size_t recordSize = 71;
void pathFor(char* path, size_t size, unsigned slot) { snprintf(path, size, "/crossink/transit/stop-%02u.dat", slot); }
uint32_t checksum(const uint8_t* bytes) {
  uint32_t value = 2166136261u;
  for (unsigned i = 0; i < recordSize - 4; ++i) value = (value ^ bytes[i]) * 16777619u;
  return value;
}
bool validMac(const uint8_t* mac) {
  uint8_t any = 0;
  for (unsigned i = 0; i < 6; ++i) any |= mac[i];
  return any && !(mac[0] & 1);
}
}  // namespace
void TransitAlertActivity::onEnter() {
  Activity::onEnter();
  load();
  requestUpdate();
}
void TransitAlertActivity::onExit() {
  Activity::onExit();
  stop();
}
void TransitAlertActivity::stop() {
  monitoring = false;
  if (owned && RADIO.shutdown(owner)) owned = false;
}
void TransitAlertActivity::load() {
  count = 0;
  if (!Storage.ready()) {
    LOG_ERR("TRANSIT", "SD unavailable");
    storageError = true;
    return;
  }
  for (unsigned slot = 0; slot < 32; ++slot) {
    char path[40];
    pathFor(path, sizeof(path), slot);
    if (!Storage.exists(path)) continue;
    auto f = Storage.open(path);
    if (!f) {
      LOG_ERR("TRANSIT", "Cannot open stop %u", slot);
      storageError = true;
      continue;
    }
    uint8_t bytes[recordSize]{};
    bool ok = f.fileSize() == sizeof(bytes) && f.read(bytes, sizeof(bytes)) == sizeof(bytes);
    const bool closed = f.close();
    const uint32_t sum = checksum(bytes);
    ok = ok && closed && !memcmp(bytes, "TRN1", 4) && bytes[4] && bytes[35] == 0 && bytes[36] >= 1 && bytes[36] <= 5;
    for (unsigned i = 0; i < 4; ++i) ok = ok && bytes[67 + i] == uint8_t(sum >> (8 * i));
    for (unsigned i = 4; i < 35 && bytes[i]; ++i) ok = ok && bytes[i] >= 32 && bytes[i] < 127;
    for (unsigned i = 0; ok && i < bytes[36]; ++i) {
      ok = validMac(bytes + 37 + i * 6);
      for (unsigned j = 0; j < i; ++j) ok = ok && memcmp(bytes + 37 + i * 6, bytes + 37 + j * 6, 6) != 0;
    }
    if (!ok) {
      LOG_ERR("TRANSIT", "Invalid stop %u; preserving file", slot);
      storageError = true;
      continue;
    }
    auto& s = stops[count];
    memcpy(s.name, bytes + 4, sizeof(s.name));
    s.count = bytes[36];
    memcpy(s.bssids, bytes + 37, sizeof(s.bssids));
    slots[count++] = slot;
  }
}
bool TransitAlertActivity::scan() {
  {
    RenderLock lock(*this);
    busy = true;
    failed = false;
    score = 0;
  }
  if (requestUpdateAndWait() != RequestUpdateResult::Rendered) {
    LOG_ERR("TRANSIT", "Scan screen unavailable");
    RenderLock lock(*this);
    busy = false;
    failed = true;
    stop();
    requestUpdate();
    return false;
  }
  if (!owned) owned = RADIO.acquire(RadioManager::Mode::WifiScan, owner);
  int found = -1;
  if (owned && RADIO.owner() == owner && RADIO.mode() == RadioManager::Mode::WifiScan)
    found = RADIO.scanNetworks(aps, 20, true);
  RenderLock lock(*this);
  busy = false;
  fingerprintCount = 0;
  memset(fingerprint, 0, sizeof(fingerprint));
  lastScan = millis();
  if (found < 0) {
    LOG_ERR("TRANSIT", "Passive scan unavailable");
    failed = true;
    stop();
    requestUpdate();
    return false;
  }
  // Strongest five distinct valid BSSIDs in the bounded snapshot, for both
  // enrollment and comparison. No SSIDs, RSSI inference, or GPS claim.
  std::sort(aps, aps + found, [](const auto& a, const auto& b) { return a.rssi > b.rssi; });
  for (int i = 0; i < found && fingerprintCount < 5; ++i) {
    if (!validMac(aps[i].bssid)) continue;
    bool duplicate = false;
    for (unsigned j = 0; j < fingerprintCount; ++j) duplicate |= memcmp(fingerprint[j], aps[i].bssid, 6) == 0;
    if (!duplicate) memcpy(fingerprint[fingerprintCount++], aps[i].bssid, 6);
  }
  requestUpdate();
  return true;
}
void TransitAlertActivity::capture() {
  auto child = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_TRANSIT_NAME), "", 31);
  if (!child) {
    LOG_ERR("TRANSIT", "Name input allocation failed");
    failed = true;
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(child), [this](const ActivityResult& result) {
    if (result.isCancelled) return;
    const auto& name = std::get<KeyboardResult>(result.data).text;
    if (name.empty()) return;
    if (!scan()) return;
    RenderLock lock(*this);
    stop();
    if (!fingerprintCount || !Storage.ready() || !Storage.ensureDirectoryExists("/crossink/transit")) {
      LOG_ERR("TRANSIT", "Cannot save empty fingerprint or unavailable directory");
      failed = true;
      requestUpdate();
      return;
    }
    unsigned slot = 0;
    char path[40];
    for (; slot < 32; ++slot) {
      pathFor(path, sizeof(path), slot);
      if (!Storage.exists(path)) break;
    }
    if (slot == 32) {
      LOG_ERR("TRANSIT", "All stop slots occupied");
      storageError = true;
      requestUpdate();
      return;
    }
    uint8_t bytes[recordSize]{};
    memcpy(bytes, "TRN1", 4);
    const size_t length = std::min<size_t>(31, name.size());
    for (size_t i = 0; i < length; ++i) bytes[4 + i] = name[i] >= 32 && name[i] < 127 ? name[i] : '?';
    bytes[36] = fingerprintCount;
    memcpy(bytes + 37, fingerprint, sizeof(fingerprint));
    const uint32_t sum = checksum(bytes);
    for (unsigned i = 0; i < 4; ++i) bytes[67 + i] = uint8_t(sum >> (8 * i));
    // Exclusive immutable records preserve all existing stops, including corrupt ones.
    auto f = Storage.open(path, O_WRITE | O_CREAT | O_EXCL);
    if (!f) {
      LOG_ERR("TRANSIT", "Cannot create stop");
      storageError = true;
      requestUpdate();
      return;
    }
    bool ok = f.write(bytes, sizeof(bytes)) == sizeof(bytes);
    if (ok) ok = f.sync();
    const bool closed = f.close();
    if (!ok || !closed) {
      LOG_ERR("TRANSIT", "Stop write/sync/close failed");
      if (!Storage.remove(path)) LOG_ERR("TRANSIT", "Partial stop cleanup failed");
      storageError = true;
    } else {
      load();
      for (unsigned i = 0; i < count; ++i)
        if (slots[i] == slot) selected = i;
    }
    requestUpdate();
  });
}
void TransitAlertActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
      TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    if (monitoring || alert) {
      RenderLock lock(*this);
      stop();
      alert = false;
      requestUpdate();
    } else
      finish();
    return;
  }
  if (monitoring) {
    if (uint32_t(millis() - started) >= 30u * 60000u) {
      RenderLock lock(*this);
      stop();
      requestUpdate();
      return;
    }
    if (uint32_t(millis() - lastScan) >= 15000u) {
      if (!scan()) return;
      RenderLock lock(*this);
      unsigned matches = 0;
      const auto& target = stops[selected];
      for (unsigned i = 0; i < target.count; ++i)
        for (unsigned j = 0; j < fingerprintCount; ++j)
          if (!memcmp(target.bssids[i], fingerprint[j], 6)) {
            ++matches;
            break;
          }
      const unsigned combined = target.count + fingerprintCount - matches;
      score = combined ? matches * 100 / combined : 0;
      if (score >= 60) {
        stop();
        alert = true;
      }
      requestUpdate();
    }
    return;
  }
  RenderLock lock(*this);
  if (alert) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      alert = false;
      score = 0;
      monitoring = true;
      started = millis();
      lastScan = started - 15000u;
      requestUpdate();
    }
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Left) ||
      mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    selected = (selected + (mappedInput.wasPressed(MappedInputManager::Button::Right) ? 1 : count)) % (count + 1);
    failed = false;
    requestUpdate();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (selected == count)
      capture();
    else {
      score = 0;
      failed = false;
      monitoring = true;
      started = millis();
      lastScan = started - 15000u;
      requestUpdate();
    }
  }
}
void TransitAlertActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  GUI.drawHeader(renderer, header, tr(STR_TRANSIT_APP));
  const auto area = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  int y = area.y + line;
  auto draw = [&](const char* s) {
    UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID, y, s);
    y += line;
  };
  char text[96];
  draw(tr(STR_TRANSIT_HINT));
  draw(tr(STR_TRANSIT_LIMIT));
  if (busy)
    draw(tr(STR_SCANNING));
  else if (failed)
    draw(tr(STR_TRANSIT_FAILED));
  else if (alert)
    draw(tr(STR_TRANSIT_MATCH));
  else if (monitoring)
    draw(tr(STR_TRANSIT_RUNNING));
  if (selected < count) {
    draw(stops[selected].name);
    snprintf(text, sizeof(text), tr(STR_TRANSIT_SCORE), score, unsigned(stops[selected].count));
    draw(text);
  } else
    draw(tr(STR_TRANSIT_CAPTURE));
  if (!monitoring && !alert) {
    snprintf(text, sizeof(text), "%u / %u", selected + 1, count + 1);
    draw(text);
    draw(tr(STR_TRANSIT_SELECT));
  }
  if (storageError) draw(tr(STR_TRANSIT_STORAGE));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
