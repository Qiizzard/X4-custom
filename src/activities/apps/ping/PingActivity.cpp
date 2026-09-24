#include "PingActivity.h"

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
constexpr char kOwner[] = "ping_tcp";
}

void PingActivity::onEnter() {
  Activity::onEnter();
  owned = RADIO.acquire(RadioManager::Mode::WifiStation, kOwner);
  if (!owned) {
    state = State::Failed;
    requestUpdate();
    return;
  }
  auto picker = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput, true, false, kOwner);
  if (!picker) {
    LOG_ERR("PING", "Picker allocation failed");
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
void PingActivity::onExit() {
  Activity::onExit();
  if (owned && RADIO.shutdown(kOwner)) owned = false;
}
void PingActivity::prompt() {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_APP_PING_TCP), hostname, 253);
  if (!keyboard) {
    LOG_ERR("PING", "Keyboard allocation failed");
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
      LOG_ERR("PING", "Invalid hostname length");
      state = State::Failed;
      requestUpdate();
      return;
    }
    memcpy(hostname, input.c_str(), input.size() + 1);
    start();
  });
}
void PingActivity::start() {
  {
    RenderLock lock(*this);
    state = State::Resolving;
    address[0] = 0;
  }
  if (requestUpdateAndWait() != RequestUpdateResult::Rendered) {
    LOG_ERR("PING", "Lookup screen unavailable");
    RenderLock lock(*this);
    state = State::Failed;
    requestUpdate();
    return;
  }
  char resolved[48] = {};
  const bool ok = RADIO.resolveHostname(kOwner, hostname, resolved);
  {
    RenderLock lock(*this);
    memcpy(address, resolved, sizeof(address));
    attempts = connected = 0;
    totalMs = minMs = maxMs = lastMs = 0;
    nextProbe = millis();
    state = ok ? State::Running : State::Failed;
  }
  requestUpdate();
}
void PingActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (state == State::Running) {
      RenderLock lock(*this);
      state = State::Summary;
      requestUpdate();
    } else
      finish();
    return;
  }
  if (owned && (state == State::Summary || state == State::Failed) &&
      mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    prompt();
    return;
  }
  if (state != State::Running || int32_t(millis() - nextProbe) < 0) return;
  uint32_t elapsed = 0;
  const int result = RADIO.probeTcp(kOwner, address, 80, 1000, elapsed);
  {
    RenderLock lock(*this);
    if (result < 0) {
      state = State::Failed;
    } else {
      ++attempts;
      lastConnected = result == 1;
      lastMs = elapsed;
      if (lastConnected) {
        ++connected;
        totalMs += elapsed;
        if (connected == 1 || elapsed < minMs) minMs = elapsed;
        if (elapsed > maxMs) maxMs = elapsed;
      }
      if (attempts == 5) state = State::Summary;
      nextProbe = millis() + 1500;
    }
  }
  requestUpdate();
}
void PingActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_PING_TCP), false);
  else
    GUI.drawHeader(renderer, header, tr(STR_APP_PING_TCP));
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  int y = screen.y + line;
  char text[96];
  snprintf(text, sizeof(text), "%.32s", hostname);
  UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, text);
  y += line;
  if (state == State::Running || state == State::Summary) {
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, tr(STR_TCP_PORT80));
    y += line;
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, address);
    y += line;
    snprintf(text, sizeof(text), tr(STR_TCP_COUNTS), unsigned(attempts), unsigned(connected));
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, text);
    y += line;
    if (attempts) {
      if (lastConnected)
        snprintf(text, sizeof(text), tr(STR_TCP_CONNECTED_MS), static_cast<unsigned long>(lastMs));
      else
        snprintf(text, sizeof(text), "%s", tr(STR_TCP_NO_CONNECTION));
      UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, text);
      y += line;
    }
    if (connected) {
      snprintf(text, sizeof(text), tr(STR_TCP_TIMINGS), static_cast<unsigned long>(minMs),
               static_cast<unsigned long>(totalMs / connected), static_cast<unsigned long>(maxMs));
      UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, text);
    }
  } else {
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y,
                              state == State::Resolving ? tr(STR_DNS_RESOLVING)
                              : state == State::Failed  ? tr(STR_TCP_FAILED)
                                                        : tr(STR_LOADING));
  }
  const auto labels = mappedInput.mapLabels(
      tr(STR_BACK), owned && (state == State::Summary || state == State::Failed) ? tr(STR_DNS_NEW_LOOKUP) : "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
