#include "DnsLookupActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <RadioManager.h>

#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr char kOwner[] = "dns_lookup";
}

void DnsLookupActivity::onEnter() {
  Activity::onEnter();
  owned = RADIO.acquire(RadioManager::Mode::WifiStation, kOwner);
  if (!owned) {
    state = State::Failed;
    requestUpdate();
    return;
  }
  auto picker = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput, true, false, kOwner);
  if (!picker) {
    LOG_ERR("DNS", "Picker allocation failed");
    state = State::Failed;
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(picker), [this](const ActivityResult& result) {
    if (result.isCancelled)
      finish();
    else
      prompt();
  });
}
void DnsLookupActivity::onExit() {
  Activity::onExit();
  if (owned && RADIO.shutdown(kOwner)) owned = false;
}
void DnsLookupActivity::prompt() {
  auto keyboard =
      makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_APP_DNS_LOOKUP), hostname, 253);
  if (!keyboard) {
    LOG_ERR("DNS", "Keyboard allocation failed");
    state = State::Failed;
    requestUpdate();
    return;
  }
  state = State::Waiting;
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    if (result.isCancelled) {
      finish();
      return;
    }
    const auto& input = std::get<KeyboardResult>(result.data).text;
    if (input.empty() || input.size() >= sizeof(hostname)) {
      LOG_ERR("DNS", "Invalid hostname length");
      state = State::Failed;
      requestUpdate();
      return;
    }
    memcpy(hostname, input.c_str(), input.size() + 1);
    resolve();
  });
}
void DnsLookupActivity::resolve() {
  {
    RenderLock lock(*this);
    state = State::Resolving;
    address[0] = 0;
  }
  if (requestUpdateAndWait() != RequestUpdateResult::Rendered) {
    LOG_ERR("DNS", "Lookup screen unavailable");
    RenderLock lock(*this);
    state = State::Failed;
    requestUpdate();
    return;
  }
  // The activity owns fixed input/output storage; SDK resolver work is synchronous.
  char resolved[48] = {};
  const bool ok = RADIO.resolveHostname(kOwner, hostname, resolved);
  {
    RenderLock lock(*this);
    memcpy(address, resolved, sizeof(address));
    state = ok ? State::Result : State::Failed;
  }
  requestUpdate();
}
void DnsLookupActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (owned && (state == State::Result || state == State::Failed) &&
      mappedInput.wasPressed(MappedInputManager::Button::Confirm))
    prompt();
}
void DnsLookupActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_DNS_LOOKUP), false);
  else
    GUI.drawHeader(renderer, header, tr(STR_APP_DNS_LOOKUP));
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  const int y = screen.y + screen.height / 2 - line;
  const char* message = state == State::Resolving ? tr(STR_DNS_RESOLVING)
                        : state == State::Result  ? address
                        : state == State::Failed  ? tr(STR_DNS_FAILED)
                                                  : tr(STR_LOADING);
  UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, message);
  if (state == State::Result) {
    char preview[37];
    snprintf(preview, sizeof(preview), "%.32s%s", hostname, strlen(hostname) > 32 ? "..." : "");
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y + line, preview);
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_DNS_NEW_LOOKUP), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
