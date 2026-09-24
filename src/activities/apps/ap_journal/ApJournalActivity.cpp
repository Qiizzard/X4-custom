#include "ApJournalActivity.h"

#include <GfxRenderer.h>
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
constexpr char kHistory[] = "ap_history", kWardrive[] = "wardriving", kChanges[] = "network_change";
constexpr char kHeatMap[] = "wifi_heatmap", kWatch[] = "perimeter_watch";
constexpr uint32_t kIntervals[] = {60000, 300000, 600000, 1800000};
constexpr uint32_t kFileCap = 1024 * 1024;
uint32_t crcUpdate(uint32_t crc, const uint8_t* bytes, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    crc ^= bytes[i];
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);
  }
  return crc;
}
void put32(uint8_t* p, uint32_t n) {
  for (int i = 0; i < 4; ++i) p[i] = uint8_t(n >> (8 * i));
}
uint32_t get32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
bool csvSsid(HalFile& file, const char* value) {
  char out[68];
  size_t n = 0;
  out[n++] = '"';
  const char* first = value;
  while (*first == ' ') ++first;
  if (*first && strchr("=+-@", *first)) out[n++] = '\'';
  for (const char* c = value; *c; ++c) {
    if (*c == '"') out[n++] = '"';
    out[n++] = *c;
  }
  out[n++] = '"';
  return file.write(out, n) == n;
}
}  // namespace
const char* ApJournalActivity::owner() const {
  switch (kind) {
    case Kind::History:
      return kHistory;
    case Kind::Wardriving:
      return kWardrive;
    case Kind::HeatMap:
      return kHeatMap;
    case Kind::Watch:
      return kWatch;
    case Kind::Changes:
      return kChanges;
  }
  return kChanges;
}
uint32_t ApJournalActivity::pauseMs() const {
  return kind == Kind::History   ? kIntervals[interval]
         : kind == Kind::HeatMap ? 5000
         : kind == Kind::Watch   ? 60000
                                 : 10000;
}
void ApJournalActivity::onEnter() {
  Activity::onEnter();
  owned = RADIO.acquire(RadioManager::Mode::WifiScan, owner());
  failed = !owned;
  if (kind == Kind::Changes) baseline = loadBaseline();
  requestUpdate();
}
void ApJournalActivity::onExit() {
  Activity::onExit();
  logging = false;
  if (owned && RADIO.shutdown(owner())) owned = false;
}
void ApJournalActivity::scan(bool replaceBaseline) {
  {
    RenderLock lock(*this);
    busy = true;
    failed = false;
  }
  if (requestUpdateAndWait() != RequestUpdateResult::Rendered) {
    LOG_ERR("APLOG", "Scan screen unavailable");
    RenderLock lock(*this);
    busy = false;
    failed = true;
    logging = false;
    requestUpdate();
    return;
  }
  int result = -1;
  if (owned && RADIO.owner() == owner() && RADIO.mode() == RadioManager::Mode::WifiScan)
    result = RADIO.scanNetworks(current, 40, true);
  else
    LOG_ERR("APLOG", "Scan owner lost");
  RenderLock lock(*this);
  busy = false;
  if (result < 0) {
    failed = true;
    logging = false;
    requestUpdate();
    return;
  }
  found = 0;
  for (int i = 0; i < result; ++i) {
    bool duplicate = false;
    for (int j = 0; j < found; ++j)
      if (memcmp(current[i].bssid, current[j].bssid, 6) == 0) duplicate = true;
    if (duplicate) continue;
    if (i != found) current[found] = current[i];
    for (char* c = current[found].ssid; *c; ++c)
      if (static_cast<unsigned char>(*c) < 32 || static_cast<unsigned char>(*c) >= 127) *c = '?';
    ++found;
  }
  ++scans;
  const uint32_t now = millis();
  if (kind == Kind::Changes || kind == Kind::Watch) {
    if (replaceBaseline) {
      if (kind == Kind::Watch || saveBaseline()) {
        recordCount = found;
        for (int i = 0; i < found; ++i) records[i].ap = current[i];
        baseline = true;
        baselineCount = found;
        if (kind == Kind::Watch) {
          logging = true;
          missed = 0;
        }
        differenceCount = 0;
        selected = 0;
      } else
        failed = true;
    } else if (kind == Kind::Watch)
      updateWatch(now);
    else
      compare();
    if (kind == Kind::Watch) nextScan = millis() + pauseMs();
  } else {
    if (!appendLog(now)) {
      failed = !full;
      logging = false;
      if (!full) path[0] = 0;  // Retry starts a fresh journal; earlier partial file is retained.
    }
    nextScan = millis() + pauseMs();
  }
  requestUpdate();
}
void ApJournalActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  const bool next = mappedInput.wasPressed(MappedInputManager::Button::Right);
  const bool previous = mappedInput.wasPressed(MappedInputManager::Button::Left);
  const int count = kind == Kind::Changes ? differenceCount
                    : kind == Kind::Watch ? recordCount - baselineCount
                                          : recordCount;
  if ((next || previous) && count) {
    RenderLock lock(*this);
    selected = (selected + (next ? 1 : count - 1)) % count;
    requestUpdate();
  }
  if (kind == Kind::History && !logging && scans == 0 &&
      (mappedInput.wasPressed(MappedInputManager::Button::Up) ||
       mappedInput.wasPressed(MappedInputManager::Button::Down))) {
    RenderLock lock(*this);
    interval = (interval + (mappedInput.wasPressed(MappedInputManager::Button::Down) ? 1 : 3)) % 4;
    requestUpdate();
  }
  if (!owned) return;
  if (kind == Kind::Watch) {
    if (mappedInput.wasPressed(MappedInputManager::Button::PageBack)) {
      scan(true);
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::PageForward) && recordCount > baselineCount) {
      RenderLock lock(*this);
      failed = !exportWatch();
      requestUpdate();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      if (!baseline) {
        scan(true);
        return;
      }
      RenderLock lock(*this);
      logging = !logging;
      failed = false;
      nextScan = millis();
      requestUpdate();
    }
    if (logging && int32_t(millis() - nextScan) >= 0) scan();
    return;
  }
  if (kind == Kind::Changes) {
    if (mappedInput.wasPressed(MappedInputManager::Button::PageBack))
      scan(true);
    else if (mappedInput.wasPressed(MappedInputManager::Button::Confirm))
      scan(!baseline);
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    RenderLock lock(*this);
    if (logging)
      logging = false;
    else if (!full) {
      if (!path[0] && !createLog())
        failed = true;
      else {
        logging = true;
        failed = false;
        nextScan = millis();
      }
    }
    requestUpdate();
  }
  if (logging && int32_t(millis() - nextScan) >= 0) scan();
}
bool ApJournalActivity::createLog() {
  constexpr char dir[] = "/crossink/aplogs";
  if (!Storage.exists(dir) && !Storage.mkdir(dir, true)) {
    LOG_ERR("APLOG", "Log directory failed");
    return false;
  }
  HalFile file;
  for (int i = 0; i < 100; ++i) {
    snprintf(path, sizeof(path), "%s/%s-%02d.csv", dir,
             kind == Kind::History   ? "history"
             : kind == Kind::HeatMap ? "heatmap"
                                     : "wardrive",
             i);
    if (Storage.exists(path)) continue;
    file = Storage.open(path, O_WRITE | O_CREAT | O_EXCL);
    break;
  }
  if (!file) {
    LOG_ERR("APLOG", "Log open failed or slots exhausted");
    path[0] = 0;
    return false;
  }
  static constexpr char header[] = "uptime_ms,ssid,bssid,rssi_dbm,channel,protected\n";
  bool ok = file.write(header, sizeof(header) - 1) == sizeof(header) - 1;
  if (ok) ok = file.sync();
  const bool closed = file.close();
  if (!ok || !closed) {
    LOG_ERR("APLOG", "Log header write/sync/close failed");
    if (!Storage.remove(path)) LOG_ERR("APLOG", "Partial log cleanup failed");
    path[0] = 0;
    return false;
  }
  recordCount = selected = 0;
  scans = missed = rowsWritten = 0;
  return true;
}
bool ApJournalActivity::appendLog(uint32_t now) {
  HalFile file = Storage.open(path, O_WRITE | O_APPEND);
  if (!file) {
    LOG_ERR("APLOG", "Log append open failed");
    return false;
  }
  bool ok = true;
  char text[96];
  for (int i = 0; ok && i < found; ++i) {
    const Ap& ap = current[i];
    int match = 0;
    for (; match < recordCount; ++match)
      if (memcmp(records[match].ap.bssid, ap.bssid, 6) == 0) break;
    const bool fresh = match == recordCount;
    if (fresh && recordCount == 64) {
      ++missed;
      continue;
    }
    if (fresh) {
      records[match].first = now;
      records[match].hits = 0;
      ++recordCount;
    }
    Record& record = records[match];
    record.ap = ap;
    record.last = now;
    if (record.hits != UINT32_MAX) ++record.hits;
    if (kind == Kind::Wardriving && !fresh) continue;
    // Worst-case row is below 160 bytes; stop before a row could cross the cap.
    if (file.size() + 160 > kFileCap || (kind == Kind::HeatMap && rowsWritten >= 10000)) {
      full = true;
      ok = false;
      break;
    }
    snprintf(text, sizeof(text), "%lu,", static_cast<unsigned long>(now));
    ok = file.write(text, strlen(text)) == strlen(text) && csvSsid(file, ap.ssid);
    snprintf(text, sizeof(text), ",%02X:%02X:%02X:%02X:%02X:%02X,%d,%u,%u\n", ap.bssid[0], ap.bssid[1], ap.bssid[2],
             ap.bssid[3], ap.bssid[4], ap.bssid[5], int(ap.rssi), unsigned(ap.channel), unsigned(ap.encrypted));
    ok = ok && file.write(text, strlen(text)) == strlen(text);
    if (ok) ++rowsWritten;
  }
  const bool synced = file.sync();
  const bool closed = file.close();
  if (!ok || !synced || !closed) {
    LOG_ERR("APLOG", "Log capped or write/sync/close failed; retained file may be partial");
    return false;
  }
  return true;
}
void ApJournalActivity::compare() {
  differenceCount = selected = 0;
  for (int i = 0; i < found; ++i) {
    int j = 0;
    for (; j < recordCount; ++j)
      if (memcmp(current[i].bssid, records[j].ap.bssid, 6) == 0) break;
    if (j == recordCount)
      differences[differenceCount++] = {uint8_t(i), 0};
    else if (current[i].channel != records[j].ap.channel || current[i].encrypted != records[j].ap.encrypted ||
             strcmp(current[i].ssid, records[j].ap.ssid) != 0)
      differences[differenceCount++] = {uint8_t(i), 2};
  }
  for (int j = 0; j < recordCount; ++j) {
    int i = 0;
    for (; i < found; ++i)
      if (memcmp(current[i].bssid, records[j].ap.bssid, 6) == 0) break;
    if (i == found) differences[differenceCount++] = {uint8_t(j), 1};
  }
}
void ApJournalActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const char* title = kind == Kind::History      ? tr(STR_APP_AP_HISTORY)
                      : kind == Kind::Wardriving ? tr(STR_APP_WARDRIVING)
                      : kind == Kind::HeatMap    ? tr(STR_APP_WIFI_HEATMAP)
                      : kind == Kind::Watch      ? tr(STR_APP_PERIMETER_WATCH)
                                                 : tr(STR_APP_NETWORK_CHANGE);
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::draw(renderer, header, title, false);
  else
    GUI.drawHeader(renderer, header, title);
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
  else if (failed)
    draw(tr(STR_AP_LOG_FAILED));
  else {
    draw(tr(STR_AP_SNAPSHOT_LIMIT));
    if (kind == Kind::Watch) {
      draw(!baseline ? tr(STR_WATCH_BASELINE) : logging ? tr(STR_AP_LOG_RUNNING) : tr(STR_AP_LOG_PAUSED));
      snprintf(text, sizeof(text), tr(STR_WATCH_COUNTS), recordCount - baselineCount, 64 - baselineCount,
               static_cast<unsigned long>(missed));
      draw(text);
      if (recordCount > baselineCount) {
        const Record& record = records[baselineCount + selected];
        draw(record.ap.ssid[0] ? record.ap.ssid : tr(STR_WIFI_SCAN_HIDDEN));
        snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X", record.ap.bssid[0], record.ap.bssid[1],
                 record.ap.bssid[2], record.ap.bssid[3], record.ap.bssid[4], record.ap.bssid[5]);
        draw(text);
        snprintf(text, sizeof(text), tr(STR_AP_TIMES), static_cast<unsigned long>(record.first),
                 static_cast<unsigned long>(record.last));
        draw(text);
      }
      draw(tr(STR_WATCH_CONTROLS));
      draw(tr(STR_WATCH_LIMIT));
      if (path[0]) draw(path);
    } else if (kind == Kind::Changes) {
      draw(baseline ? tr(STR_AP_COMPARE_HINT) : tr(STR_AP_BASELINE_HINT));
      snprintf(text, sizeof(text), tr(STR_AP_DIFFERENCES), differenceCount);
      draw(text);
      if (differenceCount) {
        const Difference& change = differences[selected];
        const Ap& ap = change.kind == 1 ? records[change.index].ap : current[change.index];
        draw(change.kind == 0 ? tr(STR_AP_NEW) : change.kind == 1 ? tr(STR_AP_ABSENT) : tr(STR_AP_CHANGED));
        draw(ap.ssid[0] ? ap.ssid : tr(STR_WIFI_SCAN_HIDDEN));
        snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X", ap.bssid[0], ap.bssid[1], ap.bssid[2],
                 ap.bssid[3], ap.bssid[4], ap.bssid[5]);
        draw(text);
      }
    } else {
      draw(full ? tr(STR_AP_LOG_FULL) : logging ? tr(STR_AP_LOG_RUNNING) : tr(STR_AP_LOG_PAUSED));
      snprintf(text, sizeof(text), tr(STR_AP_LOG_COUNTS), static_cast<unsigned long>(scans), recordCount,
               static_cast<unsigned long>(missed));
      draw(text);
      snprintf(text, sizeof(text), tr(STR_AP_INTERVAL), unsigned(pauseMs() / 1000));
      draw(text);
      if (recordCount) {
        const Record& record = records[selected];
        draw(record.ap.ssid[0] ? record.ap.ssid : tr(STR_WIFI_SCAN_HIDDEN));
        snprintf(text, sizeof(text), tr(STR_AP_RECORD), int(record.ap.rssi), unsigned(record.ap.channel),
                 static_cast<unsigned long>(record.hits));
        draw(text);
        snprintf(text, sizeof(text), tr(STR_AP_TIMES), static_cast<unsigned long>(record.first),
                 static_cast<unsigned long>(record.last));
        draw(text);
      }
      if (path[0]) draw(path);
      draw(kind == Kind::HeatMap ? tr(STR_HEATMAP_LIMIT) : tr(STR_AP_NO_LOCATION));
    }
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

bool ApJournalActivity::saveBaseline() {
  constexpr char dir[] = "/crossink/snapshots";
  if (!Storage.exists(dir) && !Storage.mkdir(dir, true)) {
    LOG_ERR("APLOG", "Snapshot directory failed");
    return false;
  }
  int newest = -1;
  for (int slot = 0; slot < 100; ++slot) {
    snprintf(path, sizeof(path), "%s/base-%02d.bin", dir, slot);
    if (Storage.exists(path)) newest = slot;
  }
  if (newest == 99) {
    LOG_ERR("APLOG", "Snapshot slots exhausted");
    return false;
  }
  snprintf(path, sizeof(path), "%s/base-%02d.bin", dir, newest + 1);
  HalFile file = Storage.open(path, O_WRITE | O_CREAT | O_EXCL);
  if (!file) {
    LOG_ERR("APLOG", "Snapshot open failed");
    return false;
  }
  const uint8_t header[8] = {'X', '4', 'N', 'C', 1, uint8_t(found), 0, 0};
  uint32_t crc = crcUpdate(0xffffffffu, header, sizeof(header));
  bool ok = file.write(header, sizeof(header)) == sizeof(header);
  uint8_t row[42];
  for (int i = 0; ok && i < found; ++i) {
    const Ap& ap = current[i];
    memset(row, 0, sizeof(row));
    memcpy(row, ap.ssid, strnlen(ap.ssid, 32));
    memcpy(row + 33, ap.bssid, 6);
    row[39] = uint8_t(ap.rssi);
    row[40] = ap.channel;
    row[41] = ap.encrypted ? 1 : 0;
    crc = crcUpdate(crc, row, sizeof(row));
    ok = file.write(row, sizeof(row)) == sizeof(row);
  }
  uint8_t checksum[4];
  put32(checksum, ~crc);
  ok = ok && file.write(checksum, sizeof(checksum)) == sizeof(checksum);
  if (ok) ok = file.sync();
  const bool closed = file.close();
  if (!ok || !closed) {
    LOG_ERR("APLOG", "Snapshot write/sync/close failed");
    if (!Storage.remove(path)) LOG_ERR("APLOG", "Partial snapshot cleanup failed");
    return false;
  }
  return true;
}
bool ApJournalActivity::loadBaseline() {
  // Highest numbered valid slot; malformed/incomplete files never become baseline.
  char candidate[48];
  for (int slot = 99; slot >= 0; --slot) {
    snprintf(candidate, sizeof(candidate), "/crossink/snapshots/base-%02d.bin", slot);
    if (!Storage.exists(candidate)) continue;
    HalFile file = Storage.open(candidate, O_READ);
    if (!file) {
      LOG_ERR("APLOG", "Snapshot read open failed");
      continue;
    }
    uint8_t header[8] = {}, row[42];
    bool ok = file.read(header, sizeof(header)) == sizeof(header) && memcmp(header, "X4NC", 4) == 0 && header[4] == 1 &&
              header[5] <= 40 && !header[6] && !header[7] && file.size() == uint64_t(12 + 42 * header[5]);
    uint32_t crc = crcUpdate(0xffffffffu, header, sizeof(header));
    for (int i = 0; ok && i < header[5]; ++i) {
      ok = file.read(row, sizeof(row)) == sizeof(row);
      if (!ok) break;
      crc = crcUpdate(crc, row, sizeof(row));
      if (row[32] || row[40] < 1 || row[40] > 13 || row[41] > 1) {
        ok = false;
        break;
      }
      Ap& ap = records[i].ap;
      memcpy(ap.ssid, row, 33);
      memcpy(ap.bssid, row + 33, 6);
      for (char* c = ap.ssid; *c; ++c)
        if (static_cast<unsigned char>(*c) < 32 || static_cast<unsigned char>(*c) >= 127) *c = '?';
      ap.rssi = static_cast<int8_t>(row[39]);
      ap.channel = row[40];
      ap.encrypted = row[41] != 0;
      for (int j = 0; j < i; ++j)
        if (memcmp(ap.bssid, records[j].ap.bssid, 6) == 0) ok = false;
    }
    uint8_t checksum[4];
    ok = ok && file.read(checksum, sizeof(checksum)) == sizeof(checksum) && get32(checksum) == ~crc;
    const bool closed = file.close();
    if (ok && closed) {
      recordCount = header[5];
      snprintf(path, sizeof(path), "%s", candidate);
      return true;
    }
    LOG_ERR("APLOG", "Rejected malformed/partial snapshot %d", slot);
  }
  recordCount = 0;
  return false;
}

void ApJournalActivity::updateWatch(uint32_t now) {
  for (int i = 0; i < found; ++i) {
    int match = 0;
    for (; match < recordCount; ++match)
      if (memcmp(records[match].ap.bssid, current[i].bssid, 6) == 0) break;
    if (match < baselineCount) continue;
    if (match == recordCount) {
      if (recordCount == 64) {
        ++missed;
        continue;
      }
      records[match].first = now;
      records[match].hits = 0;
      ++recordCount;
    }
    records[match].ap = current[i];
    records[match].last = now;
    if (records[match].hits != UINT32_MAX) ++records[match].hits;
  }
}
bool ApJournalActivity::exportWatch() {
  constexpr char dir[] = "/crossink/aplogs";
  if (!Storage.exists(dir) && !Storage.mkdir(dir, true)) {
    LOG_ERR("APLOG", "Watch export mkdir failed");
    return false;
  }
  HalFile file;
  for (int slot = 0; slot < 100; ++slot) {
    snprintf(path, sizeof(path), "%s/watch-%02d.csv", dir, slot);
    if (Storage.exists(path)) continue;
    file = Storage.open(path, O_WRITE | O_CREAT | O_EXCL);
    break;
  }
  if (!file) {
    LOG_ERR("APLOG", "Watch export open failed or full slots");
    return false;
  }
  static constexpr char header[] = "first_uptime_ms,last_uptime_ms,ssid,bssid,rssi_dbm,channel,observations\n";
  bool ok = file.write(header, sizeof(header) - 1) == sizeof(header) - 1;
  char row[96];
  for (int i = baselineCount; ok && i < recordCount; ++i) {
    const Record& record = records[i];
    const Ap& ap = record.ap;
    snprintf(row, sizeof(row), "%lu,%lu,", static_cast<unsigned long>(record.first),
             static_cast<unsigned long>(record.last));
    ok = file.write(row, strlen(row)) == strlen(row) && csvSsid(file, ap.ssid);
    snprintf(row, sizeof(row), ",%02X:%02X:%02X:%02X:%02X:%02X,%d,%u,%lu\n", ap.bssid[0], ap.bssid[1], ap.bssid[2],
             ap.bssid[3], ap.bssid[4], ap.bssid[5], int(ap.rssi), unsigned(ap.channel),
             static_cast<unsigned long>(record.hits));
    ok = ok && file.write(row, strlen(row)) == strlen(row);
  }
  if (ok) ok = file.sync();
  const bool closed = file.close();
  if (!ok || !closed) {
    LOG_ERR("APLOG", "Watch export write/sync/close failed");
    if (!Storage.remove(path)) LOG_ERR("APLOG", "Partial watch export cleanup failed");
    return false;
  }
  return true;
}
