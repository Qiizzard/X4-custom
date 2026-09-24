#include "MdnsBrowserActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr char kOwner[] = "mdns_browser";
// DNS-SD protocol identifiers are data, not translatable UI labels.
constexpr const char* kServices[] = {"_http",  "_https",         "_printer", "_ipp", "_googlecast",
                                     "_sonos", "_homeassistant", "_mqtt",    "_ssh", "_ftp"};
constexpr int kServiceCount = sizeof(kServices) / sizeof(kServices[0]);
}  // namespace

void MdnsBrowserActivity::onEnter() {
  Activity::onEnter();
  owned = RADIO.acquire(RadioManager::Mode::WifiStation, kOwner);
  if (!owned) {
    state = State::Failed;
    requestUpdate();
    return;
  }
  auto picker = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput, true, false, kOwner);
  if (!picker) {
    LOG_ERR("MDNS", "Picker allocation failed");
    state = State::Failed;
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(picker), [this](const ActivityResult& result) {
    if (result.isCancelled) {
      finish();
      return;
    }
    state = State::Select;
    requestUpdate();
  });
}
void MdnsBrowserActivity::onExit() {
  Activity::onExit();
  if (owned && RADIO.shutdown(kOwner)) owned = false;
}
void MdnsBrowserActivity::query() {
  RenderLock lock(*this);
  state = State::Querying;
  exportStatus = ExportStatus::None;
  count = selected = 0;
  partial = false;
  queryService = service == kServiceCount ? 0 : service;
  requestUpdate();
}
void MdnsBrowserActivity::queryNext() {
  if (requestUpdateAndWait() != RequestUpdateResult::Rendered) {
    LOG_ERR("MDNS", "Query screen unavailable");
    RenderLock lock(*this);
    partial = true;
    state = count ? State::Results : State::Failed;
    requestUpdate();
    return;
  }
  // One bounded blocking query per loop; Back/global Home can run between types.
  // Render reads no records while Querying. No task survives the query return.
  const int found =
      RADIO.browseMdns(kOwner, kServices[queryService], results + count, RadioManager::kMaxMdnsResults - count);
  {
    RenderLock lock(*this);
    if (found < 0) {
      partial = true;
    } else {
      for (int i = 0; i < found; ++i) resultServices[count + i] = queryService;
      count += found;
    }
    ++queryService;
    const bool more = service == kServiceCount && queryService < kServiceCount;
    const bool connected = RADIO.stationConnected(kOwner);
    if (!more || count == RadioManager::kMaxMdnsResults || !connected) {
      if (more) partial = true;
      state = count || !partial ? State::Results : State::Failed;
    }
  }
  requestUpdate();
}
void MdnsBrowserActivity::loop() {
  if (exportStatus != ExportStatus::None) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm) ||
        mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
      RenderLock lock(*this);
      exportStatus = ExportStatus::None;
      requestUpdate();
    }
    return;
  }
  if (state == State::Results && count > 0 && mappedInput.wasPressed(MappedInputManager::Button::PageBack)) {
    int slot = -1;
    const bool saved = saveCsv(slot);
    RenderLock lock(*this);
    exportSlot = slot;
    exportStatus = saved ? ExportStatus::Saved : ExportStatus::Failed;
    requestUpdate();
    return;
  }
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (state == State::Querying) {
      RenderLock lock(*this);
      partial = true;
      state = count ? State::Results : State::Select;
      requestUpdate();
    } else if (owned && (state == State::Results || state == State::Failed)) {
      RenderLock lock(*this);
      state = State::Select;
      requestUpdate();
    } else {
      finish();
    }
    return;
  }
  if (state == State::Querying) {
    queryNext();
    return;
  }
  const bool next = mappedInput.wasPressed(MappedInputManager::Button::Right) ||
                    mappedInput.wasPressed(MappedInputManager::Button::Down);
  const bool previous = mappedInput.wasPressed(MappedInputManager::Button::Left) ||
                        mappedInput.wasPressed(MappedInputManager::Button::Up);
  if (next || previous) {
    RenderLock lock(*this);
    if (state == State::Select) service = (service + (next ? 1 : kServiceCount)) % (kServiceCount + 1);
    if (state == State::Results && count > 0) selected = (selected + (next ? 1 : count - 1)) % count;
    requestUpdate();
  }
  if (owned && (state == State::Select || state == State::Results || state == State::Failed) &&
      mappedInput.wasPressed(MappedInputManager::Button::Confirm))
    query();
}
void MdnsBrowserActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_MDNS_BROWSER), false);
  else
    GUI.drawHeader(renderer, header, tr(STR_APP_MDNS_BROWSER));
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  int y = screen.y + line;
  char text[96];
  if (state == State::Querying)
    snprintf(text, sizeof(text), "%s._tcp (%d/%d)", kServices[queryService],
             service == kServiceCount ? queryService + 1 : 1, service == kServiceCount ? kServiceCount : 1);
  else if (state == State::Results && count > 0)
    snprintf(text, sizeof(text), "%s._tcp", kServices[resultServices[selected]]);
  else if (service == kServiceCount)
    snprintf(text, sizeof(text), "%s", tr(STR_MDNS_ALL));
  else
    snprintf(text, sizeof(text), "%s._tcp", kServices[service]);
  UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, text);
  y += line;
  if (exportStatus != ExportStatus::None) {
    if (exportStatus == ExportStatus::Saved)
      snprintf(text, sizeof(text), tr(STR_MDNS_EXPORT_SAVED), exportSlot);
    else
      snprintf(text, sizeof(text), "%s", tr(STR_MDNS_EXPORT_FAILED));
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y + line, text);
  } else if (state == State::Results && count > 0) {
    const auto& item = results[selected];
    snprintf(text, sizeof(text), "%d/%d (%s)", selected + 1, count, partial ? tr(STR_MDNS_PARTIAL) : tr(STR_MDNS_CAP));
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, text);
    y += line;
    snprintf(text, sizeof(text), "%.32s", item.instance);
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, text);
    y += line;
    snprintf(text, sizeof(text), "%.32s", item.hostname);
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, text);
    y += line;
    snprintf(text, sizeof(text), "%s : %u", item.ipv4[0] ? item.ipv4 : tr(STR_MDNS_NO_IPV4), item.port);
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, text);
    y += line;
    // Address text comes from the SDK; show it whole, without inventing an IPv6 result.
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, item.ipv6[0] ? item.ipv6 : tr(STR_MDNS_NO_IPV6));
    y += line;
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, tr(STR_MDNS_EXPORT_HINT));
  } else {
    const char* message = state == State::Select     ? tr(STR_MDNS_SELECT)
                          : state == State::Querying ? tr(STR_MDNS_QUERYING)
                          : state == State::Results  ? tr(STR_MDNS_EMPTY)
                          : state == State::Failed   ? tr(STR_MDNS_FAILED)
                                                     : tr(STR_LOADING);
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y + line, message);
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK),
                                            exportStatus != ExportStatus::None ? tr(STR_BACK)
                                            : owned                            ? tr(STR_MDNS_QUERY)
                                                                               : "",
                                            tr(STR_PREV_NEXT), "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

bool MdnsBrowserActivity::saveCsv(int& slot) const {
  constexpr char directory[] = "/crossink/mdns";
  if (!Storage.exists(directory) && !Storage.mkdir(directory, true)) {
    LOG_ERR("MDNS", "Could not create export directory");
    return false;
  }
  char path[40];
  HalFile file;
  for (slot = 0; slot < 100; ++slot) {
    snprintf(path, sizeof(path), "%s/services-%02d.csv", directory, slot);
    if (Storage.exists(path)) continue;
    file = Storage.open(path, O_WRITE | O_CREAT | O_EXCL);
    break;
  }
  if (!file) {
    LOG_ERR("MDNS", "Could not create export (SD error or all slots used)");
    return false;
  }
  // One bounded field at a time, not a heap-grown row or document. Inputs are
  // fixed records (at most 64 bytes); sanitization occurred during query copy.
  auto field = [&file](const char* value) {
    char escaped[132];
    size_t used = 0;
    escaped[used++] = '"';
    const char* first = value;
    while (*first == ' ') ++first;
    if (*first && std::strchr("=+-@", *first)) escaped[used++] = '\'';
    for (const char* c = value; *c; ++c) {
      if (used + 3 >= sizeof(escaped)) return false;
      if (*c == '"') escaped[used++] = '"';
      escaped[used++] = *c;
    }
    escaped[used++] = '"';
    return file.write(escaped, used) == used;
  };
  auto separator = [&file](const char c) { return file.write(&c, 1) == 1; };
  static constexpr char header[] = "instance,hostname,ipv4,ipv6,port,service\n";
  bool ok = file.write(header, sizeof(header) - 1) == sizeof(header) - 1;
  for (int i = 0; ok && i < count; ++i) {
    const auto& item = results[i];
    char port[6];
    snprintf(port, sizeof(port), "%u", unsigned(item.port));
    char type[24];
    snprintf(type, sizeof(type), "%s._tcp", kServices[resultServices[i]]);
    ok = field(item.instance) && separator(',') && field(item.hostname) && separator(',') && field(item.ipv4) &&
         separator(',') && field(item.ipv6) && separator(',') && field(port) && separator(',') && field(type) &&
         separator('\n');
  }
  if (ok) ok = file.sync();
  const bool closed = file.close();
  if (!ok || !closed) {
    LOG_ERR("MDNS", "Export write/sync/close failed");
    if (!Storage.remove(path)) LOG_ERR("MDNS", "Could not remove partial export");
    return false;
  }
  LOG_INF("MDNS", "Saved %d service rows to %s", count, path);
  return true;
}
