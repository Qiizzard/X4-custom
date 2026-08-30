// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
#include "DiceRollerActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/ButtonNavigator.h"

void DiceRollerActivity::onEnter() {
  Activity::onEnter();
  state = State::Select;
  dieTypeIndex = 1;
  dieCount = 1;
  resultCount = 0;
  total = 0;
  // Seed from the clock. Never zero: xorshift32 is stuck at zero forever.
  rngState = static_cast<uint32_t>(millis()) * 2654435761u;
  if (rngState == 0) rngState = 0x9E3779B9u;
  requestUpdate();
}

void DiceRollerActivity::onExit() { Activity::onExit(); }

uint16_t DiceRollerActivity::rollDie() {
  rngState ^= rngState << 13;
  rngState ^= rngState >> 17;
  rngState ^= rngState << 5;
  const uint16_t sides = kDieTypes[dieTypeIndex];
  return static_cast<uint16_t>((rngState % sides) + 1);
}

void DiceRollerActivity::rollAll() {
  total = 0;
  resultCount = dieCount;
  for (uint8_t i = 0; i < resultCount; ++i) {
    results[i] = rollDie();
    total += results[i];
  }
}

void DiceRollerActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }

  if (state == State::Select) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      finish();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
      dieTypeIndex = static_cast<uint8_t>(ButtonNavigator::previousIndex(dieTypeIndex, kDieTypeCount));
      requestUpdate();
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
      dieTypeIndex = static_cast<uint8_t>(ButtonNavigator::nextIndex(dieTypeIndex, kDieTypeCount));
      requestUpdate();
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Left) && dieCount > 1) {
      --dieCount;
      requestUpdate();
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Right) && dieCount < kMaxDice) {
      ++dieCount;
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      state = State::Rolling;
      animFrame = 0;
      animStartMs = millis();
      rollAll();
      requestUpdate();
    }
    return;
  }

  if (state == State::Rolling) {
    const unsigned long elapsed = millis() - animStartMs;
    const uint8_t frame = static_cast<uint8_t>(elapsed / kAnimFrameMs);
    if (frame >= kAnimFrames) {
      rollAll();
      state = State::Result;
      requestUpdate();
    } else if (frame != animFrame) {
      animFrame = frame;
      rollAll();
      requestUpdate();
    }
    return;
  }

  // Result
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    state = State::Select;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    state = State::Rolling;
    animFrame = 0;
    animStartMs = millis();
    rollAll();
    requestUpdate();
  }
}

void DiceRollerActivity::renderSelect() const {
  const auto pageHeight = renderer.getScreenHeight();
  char line[32];

  snprintf(line, sizeof(line), "%ud%u", static_cast<unsigned>(dieCount),
           static_cast<unsigned>(kDieTypes[dieTypeIndex]));
  renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 - 30, line, true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, tr(STR_DICE_PICK_HINT), true);
}

void DiceRollerActivity::renderResult() const {
  const auto pageHeight = renderer.getScreenHeight();
  char line[64];

  // The dice themselves, on one line -- at most 6 values of at most 3 digits.
  int offset = 0;
  for (uint8_t i = 0; i < resultCount && offset < static_cast<int>(sizeof(line)) - 1; ++i) {
    offset += snprintf(line + offset, sizeof(line) - static_cast<size_t>(offset), i == 0 ? "%u" : "  %u",
                       static_cast<unsigned>(results[i]));
  }
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 40, line, true);

  snprintf(line, sizeof(line), "%lu", static_cast<unsigned long>(total));
  renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2, line, true, EpdFontFamily::BOLD);

  if (state == State::Result) {
    snprintf(line, sizeof(line), "%ud%u", static_cast<unsigned>(dieCount),
             static_cast<unsigned>(kDieTypes[dieTypeIndex]));
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 40, line, true);
  }
}

void DiceRollerActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_DICE_ROLLER), false);
  } else {
    GUI.drawHeader(renderer, header, tr(STR_APP_DICE_ROLLER));
  }

  if (state == State::Select) {
    renderSelect();
  } else {
    renderResult();
  }

  const auto labels = state == State::Select
                          ? mappedInput.mapLabels(tr(STR_BACK), tr(STR_DICE_ROLL), tr(STR_DIR_UP), tr(STR_DIR_DOWN))
                          : mappedInput.mapLabels(tr(STR_BACK), tr(STR_DICE_ROLL), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
