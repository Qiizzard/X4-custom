#include "WifiScannerActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

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
  if (exportStatus != ExportStatus::None) {
    if (millis() - exportShownAt >= 5000 || mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      RenderLock lock(*this);
      exportStatus = ExportStatus::None;
      requestUpdate();
    }
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    scan();
    return;
  }
  if (count <= 0) return;
  if (mappedInput.wasPressed(MappedInputManager::Button::PageBack)) {
    int slot = -1;
    const bool saved = saveCsv(slot);
    RenderLock lock(*this);
    exportSlot = slot;
    exportShownAt = millis();
    exportStatus = saved ? ExportStatus::Saved : ExportStatus::Failed;
    requestUpdate();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Up) ||
      mappedInput.wasPressed(MappedInputManager::Button::Down) ||
      mappedInput.wasPressed(MappedInputManager::Button::PageForward)) {
    RenderLock lock(*this);
    channelView = !channelView;
    requestUpdate();
    return;
  }
  int step = 0;
  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) step = -1;
  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) step = 1;
  if (step) {
    RenderLock lock(*this);
    if (channelView)
      selectedChannel = (selectedChannel - 1 + step + 13) % 13 + 1;
    else
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
  if (exportStatus != ExportStatus::None) {
    char text[96];
    if (exportStatus == ExportStatus::Saved)
      snprintf(text, sizeof(text), tr(STR_WIFI_SCAN_EXPORT_SAVED), exportSlot);
    else
      snprintf(text, sizeof(text), "%s", tr(STR_WIFI_SCAN_EXPORT_FAILED));
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, text);
  } else if (!owned || scanning || count <= 0) {
    const char* message = !owned      ? tr(STR_RADIO_BUSY_OR_UNAVAILABLE)
                          : scanning  ? tr(STR_SCANNING)
                          : count < 0 ? tr(STR_WIFI_SCAN_FAILED)
                                      : tr(STR_NO_NETWORKS);
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, message);
  } else if (channelView) {
    renderChannels();
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
  if (owned && !scanning && count > 0) {
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, screen.y + screen.height - 2 * line,
                              tr(STR_WIFI_SCAN_VIEW_HINT));
  }
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), tr(STR_WIFI_SCAN_RESCAN), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void WifiScannerActivity::renderChannels() const {
  // At most 40 retained APs: byte counts and signed 16-bit sums suffice.
  uint8_t counts[14] = {};
  int8_t peaks[14];
  std::fill(std::begin(peaks), std::end(peaks), -127);
  int16_t sums[14] = {};
  for (int i = 0; i < count; ++i) {
    const auto& ap = results[i];
    if (ap.channel < 1 || ap.channel > 13) continue;
    ++counts[ap.channel];
    sums[ap.channel] += ap.rssi;
    peaks[ap.channel] = std::max(peaks[ap.channel], ap.rssi);
  }
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  const int top = header.y + header.height + line;
  char text[96];
  snprintf(text, sizeof(text), tr(STR_WIFI_SCAN_CHANNEL_COUNT), selectedChannel, unsigned(counts[selectedChannel]));
  UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, top, text);
  if (counts[selectedChannel]) {
    snprintf(text, sizeof(text), tr(STR_WIFI_SCAN_CHANNEL_SIGNAL), int(peaks[selectedChannel]),
             int(sums[selectedChannel] / counts[selectedChannel]));
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, top + line, text);
  }
  const int chartTop = top + 3 * line;
  const int chartBottom = screen.y + screen.height - 4 * line;
  const int chartHeight = chartBottom - chartTop;
  const int columnWidth = (screen.width - 2 * metrics.contentSidePadding) / 13;
  if (chartHeight <= 0 || columnWidth < 3) return;
  const int highest = std::max(1, int(*std::max_element(counts + 1, counts + 14)));
  for (int channel = 1; channel <= 13; ++channel) {
    const int x = screen.x + metrics.contentSidePadding + (channel - 1) * columnWidth;
    const int height = int(counts[channel]) * chartHeight / highest;
    if (height) renderer.fillRect(x + 1, chartBottom - height, columnWidth - 2, height, true);
    snprintf(text, sizeof(text), "%d", channel);
    renderer.drawText(UI_10_FONT_ID, x, chartBottom + 4, text);
  }
  UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, chartBottom + line, tr(STR_WIFI_SCAN_CAPPED_SNAPSHOT));
}

bool WifiScannerActivity::saveCsv(int& slot) const {
  constexpr char directory[] = "/crossink/wifi";
  if (!Storage.exists(directory) && !Storage.mkdir(directory, true)) {
    LOG_ERR("WSCAN", "Could not create scan export directory");
    return false;
  }
  char path[40];
  HalFile file;
  for (slot = 0; slot < 100; ++slot) {
    snprintf(path, sizeof(path), "%s/scan-%02d.csv", directory, slot);
    if (Storage.exists(path)) continue;
    file = Storage.open(path, O_WRITE | O_CREAT | O_EXCL);
    break;
  }
  if (!file) {
    LOG_ERR("WSCAN", "Could not create scan export (SD error or all slots used)");
    return false;
  }
  static constexpr char header[] = "ssid,bssid,rssi_dbm,channel,protected\n";
  bool ok = file.write(header, sizeof(header) - 1) == sizeof(header) - 1;
  for (int i = 0; ok && i < count; ++i) {
    const auto& ap = results[i];
    char escaped[66];
    size_t used = 0;
    const char* first = ap.ssid;
    while (*first == ' ') ++first;
    // Prevent spreadsheet import from interpreting an untrusted SSID as a formula.
    if (*first && std::strchr("=+-@", *first)) escaped[used++] = '\'';
    for (const char* c = ap.ssid; *c; ++c) {
      if (*c == '"') escaped[used++] = '"';
      escaped[used++] = *c;
    }
    escaped[used] = 0;
    char row[128];
    const int length =
        snprintf(row, sizeof(row), "\"%s\",%02X:%02X:%02X:%02X:%02X:%02X,%d,%u,%u\n", escaped, unsigned(ap.bssid[0]),
                 unsigned(ap.bssid[1]), unsigned(ap.bssid[2]), unsigned(ap.bssid[3]), unsigned(ap.bssid[4]),
                 unsigned(ap.bssid[5]), int(ap.rssi), unsigned(ap.channel), unsigned(ap.encrypted));
    ok = length > 0 && size_t(length) < sizeof(row) && file.write(row, size_t(length)) == size_t(length);
  }
  if (ok) ok = file.sync();
  const bool closed = file.close();
  if (!ok || !closed) {
    LOG_ERR("WSCAN", "Scan export write/sync/close failed");
    if (!Storage.remove(path)) LOG_ERR("WSCAN", "Could not remove partial scan export");
    return false;
  }
  LOG_INF("WSCAN", "Saved %d scan rows to %s", count, path);
  return true;
}
