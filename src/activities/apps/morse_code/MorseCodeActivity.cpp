// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
#include "MorseCodeActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

void MorseCodeActivity::onEnter() {
  Activity::onEnter();
  mode = Mode::TextToMorse;
  input[0] = '\0';
  output[0] = '\0';
  overflowed = false;
  requestUpdate();
}

void MorseCodeActivity::onExit() { Activity::onExit(); }

void MorseCodeActivity::recompute() {
  const size_t written = mode == Mode::TextToMorse ? morse::encode(input, output, sizeof(output))
                                                   : morse::decode(input, output, sizeof(output));
  // encode/decode return 0 rather than truncating, so an empty result with
  // non-empty input means it did not fit -- say so instead of showing blank.
  overflowed = written == 0 && input[0] != '\0';
}

void MorseCodeActivity::promptForInput() {
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(
          renderer, mappedInput, mode == Mode::TextToMorse ? tr(STR_MORSE_ENTER_TEXT) : tr(STR_MORSE_ENTER_CODE),
          std::string(input), kMaxInput),
      [this](const ActivityResult& result) {
        if (result.isCancelled) {
          requestUpdate();
          return;
        }
        const auto* keyboard = std::get_if<KeyboardResult>(&result.data);
        if (keyboard == nullptr) {
          requestUpdate();
          return;
        }
        snprintf(input, sizeof(input), "%s", keyboard->text.c_str());
        recompute();
        requestUpdate();
      });
}

void MorseCodeActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    promptForInput();
    return;
  }
  // Up/Down flips direction; the entered text carries over, which is what you
  // want when checking a decode against its encode.
  if (mappedInput.wasPressed(MappedInputManager::Button::Up) ||
      mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    mode = mode == Mode::TextToMorse ? Mode::MorseToText : Mode::TextToMorse;
    recompute();
    requestUpdate();
  }
}

void MorseCodeActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto pageHeight = renderer.getScreenHeight();

  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_MORSE_CODE), false);
  } else {
    GUI.drawHeader(renderer, header, tr(STR_APP_MORSE_CODE));
  }

  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 60,
                            mode == Mode::TextToMorse ? tr(STR_MORSE_TEXT_TO_CODE) : tr(STR_MORSE_CODE_TO_TEXT), true,
                            EpdFontFamily::BOLD);

  if (input[0] == '\0') {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_MORSE_PRESS_TO_ENTER), true);
  } else {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, input, true);
    if (overflowed) {
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 20, tr(STR_MORSE_TOO_LONG), true);
    } else {
      renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 + 20, output, true, EpdFontFamily::BOLD);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_MORSE_ENTER), tr(STR_MORSE_SWAP), tr(STR_MORSE_SWAP));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
