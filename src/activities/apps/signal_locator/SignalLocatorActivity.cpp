#include "SignalLocatorActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"
namespace {
constexpr char kOwner[] = "signal_locator";
}
void SignalLocatorActivity::onEnter() {
  Activity::onEnter();
  owned = RADIO.acquire(RadioManager::Mode::WifiScan, kOwner);
  if (owned)
    scan(false);
  else {
    state = State::Failed;
    requestUpdate();
  }
}
void SignalLocatorActivity::onExit() {
  Activity::onExit();
  if (owned && RADIO.shutdown(kOwner)) owned = false;
}
void SignalLocatorActivity::scan(bool sampling) {
  {
    RenderLock lock(*this);
    busy = true;
  }
  if (requestUpdateAndWait() != RequestUpdateResult::Rendered) {
    LOG_ERR("LOC", "Scan screen unavailable");
    RenderLock lock(*this);
    busy = false;
    state = State::Failed;
    requestUpdate();
    return;
  }
  int found = -1;
  if (owned && RADIO.owner() == kOwner && RADIO.mode() == RadioManager::Mode::WifiScan)
    found = RADIO.scanNetworks(aps, 40, true);
  else
    LOG_ERR("LOC", "Scan owner lost");
  RenderLock lock(*this);
  busy = false;
  if (found < 0) {
    state = State::Failed;
    requestUpdate();
    return;
  }
  count = found;
  if (!sampling) {
    selected = 0;
    state = State::Select;
    for (int i = 0; i < count; ++i)
      for (char* c = aps[i].ssid; *c; ++c)
        if (static_cast<unsigned char>(*c) < 32 || static_cast<unsigned char>(*c) >= 127) *c = '?';
  } else {
    Reading& reading = readings[point];
    ++reading.attempts;
    for (int i = 0; i < count; ++i)
      if (memcmp(aps[i].bssid, target, 6) == 0) {
        reading.sum += aps[i].rssi;
        ++reading.seen;
        break;
      }
    if (reading.attempts == 3) {
      if (++point == 3)
        state = State::Results;
      else
        state = State::Ready;
    }
  }
  requestUpdate();
}
void SignalLocatorActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (state == State::Measuring) {
      RenderLock lock(*this);
      state = State::Results;
      requestUpdate();
    } else
      finish();
    return;
  }
  if (!owned) return;
  if (state == State::Measuring) {
    scan(true);
    return;
  }
  if (state == State::Select && count &&
      (mappedInput.wasPressed(MappedInputManager::Button::Left) ||
       mappedInput.wasPressed(MappedInputManager::Button::Right))) {
    RenderLock lock(*this);
    selected = (selected + (mappedInput.wasPressed(MappedInputManager::Button::Right) ? 1 : count - 1)) % count;
    requestUpdate();
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::PageBack)) {
    scan(false);
    return;
  }
  if (!mappedInput.wasPressed(MappedInputManager::Button::Confirm)) return;
  if (state == State::Failed || (state == State::Select && !count)) {
    scan(false);
    return;
  }
  RenderLock lock(*this);
  if (state == State::Select && count) {
    memcpy(target, aps[selected].bssid, 6);
    memcpy(targetName, aps[selected].ssid, sizeof(targetName));
    for (auto& reading : readings) reading = {};
    point = 0;
    state = State::Ready;
  } else if (state == State::Ready)
    state = State::Measuring;
  else if (state == State::Results) {
    for (auto& reading : readings) reading = {};
    point = 0;
    state = State::Ready;
  }
  requestUpdate();
}
void SignalLocatorActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_SIGNAL_LOCATOR), false);
  else
    GUI.drawHeader(renderer, header, tr(STR_APP_SIGNAL_LOCATOR));
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  int y = screen.y + line;
  char text[112];
  auto draw = [&](const char* s) {
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, s);
    y += line;
  };
  if (busy)
    draw(tr(STR_SCANNING));
  else if (state == State::Failed)
    draw(tr(STR_WIFI_SCAN_FAILED));
  else if (state == State::Select) {
    draw(tr(STR_LOCATOR_SELECT));
    if (!count)
      draw(tr(STR_NO_NETWORKS));
    else {
      const auto& ap = aps[selected];
      draw(ap.ssid[0] ? ap.ssid : tr(STR_WIFI_SCAN_HIDDEN));
      snprintf(text, sizeof(text), "%d/%d | %02X:%02X:%02X:%02X:%02X:%02X", selected + 1, count, ap.bssid[0],
               ap.bssid[1], ap.bssid[2], ap.bssid[3], ap.bssid[4], ap.bssid[5]);
      draw(text);
    }
  } else {
    draw(targetName[0] ? targetName : tr(STR_WIFI_SCAN_HIDDEN));
    if (state == State::Ready) {
      snprintf(text, sizeof(text), tr(STR_LOCATOR_POSITION), point + 1);
      draw(text);
    } else if (state == State::Measuring)
      draw(tr(STR_SCANNING));
    int best = -1;
    for (int i = 0; i < 3; ++i) {
      const auto& reading = readings[i];
      if (reading.seen) {
        snprintf(text, sizeof(text), tr(STR_LOCATOR_READING), i + 1, int(reading.sum / reading.seen),
                 unsigned(reading.seen), unsigned(reading.attempts));
        if (best < 0 || int(reading.sum) * readings[best].seen > int(readings[best].sum) * reading.seen) best = i;
      } else
        snprintf(text, sizeof(text), tr(STR_LOCATOR_MISSING), i + 1, unsigned(reading.attempts));
      draw(text);
    }
    if (state == State::Results && best >= 0) {
      snprintf(text, sizeof(text), tr(STR_LOCATOR_STRONGEST), best + 1);
      draw(text);
    }
    draw(tr(STR_LOCATOR_LIMIT));
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
