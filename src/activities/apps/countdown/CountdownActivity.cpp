// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
#include "CountdownActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>
#include <string>

#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

void CountdownActivity::onEnter() {
  Activity::onEnter();
  state = State::Pick;
  presetIndex = 2;
  remainingMs = 0;
  lastShownSecond = UINT32_MAX;
  requestUpdate();
}

void CountdownActivity::onExit() { Activity::onExit(); }

void CountdownActivity::startSelected() {
  remainingMs = static_cast<uint32_t>(kPresetMinutes[presetIndex]) * 60u * 1000u;
  lastTickMs = millis();
  lastShownSecond = UINT32_MAX;
  state = State::Running;
  requestUpdate();
}

void CountdownActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }

  if (state == State::Running) {
    const unsigned long now = millis();
    const unsigned long elapsed = now - lastTickMs;
    lastTickMs = now;
    remainingMs = elapsed >= remainingMs ? 0 : remainingMs - static_cast<uint32_t>(elapsed);
    if (remainingMs == 0) {
      state = State::Finished;
      requestUpdate();
    } else if (remainingSeconds() != lastShownSecond) {
      lastShownSecond = remainingSeconds();
      requestUpdate();
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (state == State::Pick) {
      finish();
    } else {
      state = State::Pick;
      requestUpdate();
    }
    return;
  }

  if (state == State::Pick) {
    const auto move = [this](const int index) {
      presetIndex = static_cast<uint8_t>(index);
      requestUpdate();
    };
    buttonNavigator.onNextRelease([this, &move] { move(ButtonNavigator::nextIndex(presetIndex, kPresetCount)); });
    buttonNavigator.onPreviousRelease(
        [this, &move] { move(ButtonNavigator::previousIndex(presetIndex, kPresetCount)); });
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) startSelected();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (state == State::Running) {
      state = State::Paused;
    } else if (state == State::Paused) {
      lastTickMs = millis();
      state = State::Running;
    } else {
      state = State::Pick;
    }
    requestUpdate();
  }
}

void CountdownActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_COUNTDOWN), false);
  } else {
    GUI.drawHeader(renderer, header, tr(STR_APP_COUNTDOWN));
  }

  if (state == State::Pick) {
    const Rect content{0, header.y + header.height + metrics.verticalSpacing, pageWidth,
                       pageHeight - (header.y + header.height) - metrics.verticalSpacing - metrics.buttonHintsHeight};
    GUI.drawList(renderer, content, kPresetCount, presetIndex, [](const int index) {
      char label[24];
      snprintf(label, sizeof(label), "%u min", static_cast<unsigned>(kPresetMinutes[index]));
      return std::string(label);
    });
  } else {
    const uint32_t seconds = remainingSeconds();
    char clock[16];
    snprintf(clock, sizeof(clock), "%02u:%02u", static_cast<unsigned>(seconds / 60),
             static_cast<unsigned>(seconds % 60));
    renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 - 10, clock, true, EpdFontFamily::BOLD);

    const char* status = state == State::Paused     ? tr(STR_COUNTDOWN_PAUSED)
                         : state == State::Finished ? tr(STR_COUNTDOWN_DONE)
                                                    : tr(STR_COUNTDOWN_RUNNING);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 30, status, true);
  }

  const char* confirmLabel = state == State::Pick      ? tr(STR_COUNTDOWN_START)
                             : state == State::Running ? tr(STR_COUNTDOWN_PAUSE)
                             : state == State::Paused  ? tr(STR_COUNTDOWN_RESUME)
                                                       : tr(STR_COUNTDOWN_RESET);
  const auto labels = state == State::Pick
                          ? mappedInput.mapLabels(tr(STR_BACK), confirmLabel, tr(STR_DIR_UP), tr(STR_DIR_DOWN))
                          : mappedInput.mapLabels(tr(STR_BACK), confirmLabel, "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
