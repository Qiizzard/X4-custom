#include "MdnsBrowserActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

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
  {
    RenderLock lock(*this);
    state = State::Querying;
    count = 0;
    selected = 0;
  }
  if (requestUpdateAndWait() != RequestUpdateResult::Rendered) {
    LOG_ERR("MDNS", "Query screen unavailable");
    RenderLock lock(*this);
    state = State::Failed;
    requestUpdate();
    return;
  }
  // Render reads no records while Querying. No background query survives exit.
  const int found = RADIO.browseMdns(kOwner, kServices[service], results, RadioManager::kMaxMdnsResults);
  {
    RenderLock lock(*this);
    count = found;
    state = found < 0 ? State::Failed : State::Results;
  }
  requestUpdate();
}
void MdnsBrowserActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (owned && (state == State::Results || state == State::Failed)) {
      RenderLock lock(*this);
      state = State::Select;
      requestUpdate();
    } else {
      finish();
    }
    return;
  }
  const bool next = mappedInput.wasPressed(MappedInputManager::Button::Right) ||
                    mappedInput.wasPressed(MappedInputManager::Button::Down);
  const bool previous = mappedInput.wasPressed(MappedInputManager::Button::Left) ||
                        mappedInput.wasPressed(MappedInputManager::Button::Up);
  if (next || previous) {
    RenderLock lock(*this);
    if (state == State::Select) service = (service + (next ? 1 : kServiceCount - 1)) % kServiceCount;
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
  snprintf(text, sizeof(text), "%s._tcp", kServices[service]);
  UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, text);
  y += line;
  if (state == State::Results && count > 0) {
    const auto& item = results[selected];
    snprintf(text, sizeof(text), "%d/%d (%s)", selected + 1, count, tr(STR_MDNS_CAP));
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
  } else {
    const char* message = state == State::Select     ? tr(STR_MDNS_SELECT)
                          : state == State::Querying ? tr(STR_MDNS_QUERYING)
                          : state == State::Results  ? tr(STR_MDNS_EMPTY)
                          : state == State::Failed   ? tr(STR_MDNS_FAILED)
                                                     : tr(STR_LOADING);
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y + line, message);
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), owned ? tr(STR_MDNS_QUERY) : "", tr(STR_PREV_NEXT), "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
