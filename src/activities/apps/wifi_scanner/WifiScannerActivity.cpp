#include "WifiScannerActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr char kOwner[] = "wifi_scanner";
}

void WifiScannerActivity::onEnter() {
  Activity::onEnter();
  owned = RADIO.acquire(RadioManager::Mode::WifiScan, kOwner);
  if (owned)
    scan();
  else
    requestUpdate();
}

void WifiScannerActivity::onExit() {
  Activity::onExit();
  if (owned && RADIO.shutdown(kOwner)) owned = false;
}

void WifiScannerActivity::scan() {
  if (!owned || RADIO.owner() != kOwner || RADIO.mode() != RadioManager::Mode::WifiScan) return;
  {
    RenderLock lock(*this);
    scanning = true;
  }
  if (requestUpdateAndWait() != RequestUpdateResult::Rendered) {
    LOG_ERR("WSCAN", "Scan screen unavailable; skipping scan");
    RenderLock lock(*this);
    scanning = false;
    count = -1;
    requestUpdate();
    return;
  }
  // Blocking SDK scan; no periodic scanning, association, or probe requests.
  const int found = RADIO.scanNetworks(results, RadioManager::kMaxScanResults, true);
  if (found > 0) {
    std::sort(results, results + found, [](const auto& a, const auto& b) { return a.rssi > b.rssi; });
    for (int i = 0; i < found; ++i) {
      for (char& c : results[i].ssid) {
        if (!c) break;
        if (static_cast<unsigned char>(c) < 32 || c == 127) c = ' ';
      }
    }
  }
  {
    RenderLock lock(*this);
    count = found;
    selected = 0;
    scanning = false;
  }
  requestUpdate();
}

void WifiScannerActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (!owned) return;
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    scan();
    return;
  }
  if (count <= 0) return;
  int step = 0;
  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) step = -1;
  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) step = 1;
  if (step) {
    RenderLock lock(*this);
    selected = (selected + step + count) % count;
    requestUpdate();
  }
}

void WifiScannerActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_WIFI_SCANNER), false);
  else
    GUI.drawHeader(renderer, header, tr(STR_APP_WIFI_SCANNER));
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  int y = screen.y + screen.height / 2 - 2 * line;
  if (!owned || scanning || count <= 0) {
    const char* message = !owned      ? tr(STR_RADIO_BUSY_OR_UNAVAILABLE)
                          : scanning  ? tr(STR_SCANNING)
                          : count < 0 ? tr(STR_WIFI_SCAN_FAILED)
                                      : tr(STR_NO_NETWORKS);
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, message);
  } else {
    const auto& entry = results[selected];
    char text[96];
    snprintf(text, sizeof(text), "%d / %d", selected + 1, count);
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, text);
    y += line;
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y,
                              entry.ssid[0] ? entry.ssid : tr(STR_WIFI_SCAN_HIDDEN));
    y += line;
    snprintf(text, sizeof(text), tr(STR_WIFI_SCAN_DETAIL), unsigned(entry.channel), int(entry.rssi),
             entry.encrypted ? tr(STR_WIFI_SCAN_PROTECTED) : tr(STR_OPEN));
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, text);
    y += line;
    snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X", unsigned(entry.bssid[0]), unsigned(entry.bssid[1]),
             unsigned(entry.bssid[2]), unsigned(entry.bssid[3]), unsigned(entry.bssid[4]), unsigned(entry.bssid[5]));
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, text);
  }
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), tr(STR_WIFI_SCAN_RESCAN), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
