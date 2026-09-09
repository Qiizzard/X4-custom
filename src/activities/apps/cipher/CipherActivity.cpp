// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
#include "CipherActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr StrId kAlgorithmTitles[] = {
    StrId::STR_CIPHER_ROT13,         StrId::STR_CIPHER_CAESAR,        StrId::STR_CIPHER_VIGENERE,
    StrId::STR_CIPHER_XOR_ENCODE,    StrId::STR_CIPHER_XOR_DECODE,    StrId::STR_CIPHER_ATBASH,
    StrId::STR_CIPHER_BASE64_ENCODE, StrId::STR_CIPHER_BASE64_DECODE,
};
static_assert(sizeof(kAlgorithmTitles) / sizeof(kAlgorithmTitles[0]) == cipher::kAlgorithmCount);
}  // namespace

void CipherActivity::onEnter() {
  Activity::onEnter();
  state = State::SelectAlgorithm;
  algorithmIndex = 0;
  inputText[0] = '\0';
  keyText[0] = '\0';
  resultText[0] = '\0';
  resultOverflowed = false;
  requestUpdate();
}

void CipherActivity::onExit() { Activity::onExit(); }

void CipherActivity::computeResult() {
  const size_t written =
      cipher::apply(cipher::kAlgorithms[algorithmIndex], inputText, keyText, resultText, sizeof(resultText));
  resultOverflowed = written == 0 && inputText[0] != '\0';
  state = State::Result;
  requestUpdate();
}

void CipherActivity::promptForKey() {
  const cipher::Algorithm algo = cipher::kAlgorithms[algorithmIndex];
  const char* prompt = algo == cipher::Algorithm::Caesar ? tr(STR_CIPHER_ENTER_SHIFT) : tr(STR_CIPHER_ENTER_KEY);
  auto keyboard =
      makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, prompt, std::string(keyText), kMaxKey);
  if (!keyboard) {
    LOG_ERR("CIPHER", "Cannot allocate keyboard (%u bytes)", static_cast<unsigned>(sizeof(KeyboardEntryActivity)));
    state = State::SelectAlgorithm;
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    const auto* keyboard = std::get_if<KeyboardResult>(&result.data);
    if (result.isCancelled || keyboard == nullptr) {
      state = State::SelectAlgorithm;
      requestUpdate();
      return;
    }
    snprintf(keyText, sizeof(keyText), "%s", keyboard->text.c_str());
    computeResult();
  });
}

void CipherActivity::promptForInput() {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_CIPHER_ENTER_TEXT),
                                                           std::string(inputText), kMaxInput);
  if (!keyboard) {
    LOG_ERR("CIPHER", "Cannot allocate keyboard (%u bytes)", static_cast<unsigned>(sizeof(KeyboardEntryActivity)));
    state = State::SelectAlgorithm;
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    const auto* keyboard = std::get_if<KeyboardResult>(&result.data);
    if (result.isCancelled || keyboard == nullptr) {
      state = State::SelectAlgorithm;
      requestUpdate();
      return;
    }
    snprintf(inputText, sizeof(inputText), "%s", keyboard->text.c_str());
    if (inputText[0] == '\0') {
      state = State::SelectAlgorithm;
      requestUpdate();
      return;
    }
    if (cipher::needsKey(cipher::kAlgorithms[algorithmIndex])) {
      promptForKey();
    } else {
      computeResult();
    }
  });
}

void CipherActivity::beginRun() {
  keyText[0] = '\0';
  promptForInput();
}

void CipherActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }

  if (state == State::SelectAlgorithm) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      finish();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
      algorithmIndex = ButtonNavigator::previousIndex(static_cast<int>(algorithmIndex), cipher::kAlgorithmCount);
      requestUpdate();
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
      algorithmIndex = ButtonNavigator::nextIndex(static_cast<int>(algorithmIndex), cipher::kAlgorithmCount);
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      beginRun();
    }
    return;
  }

  // State::Result
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    state = State::SelectAlgorithm;
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    beginRun();
  }
}

void CipherActivity::renderSelect(const Rect& header) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int disclaimerY = header.y + header.height + 4;
  const int listTop = disclaimerY + renderer.getLineHeight(SMALL_FONT_ID) + metrics.verticalSpacing;
  const int listHeight = pageHeight - listTop - metrics.buttonHintsHeight;

  renderer.drawText(SMALL_FONT_ID, metrics.contentSidePadding, disclaimerY, tr(STR_CIPHER_DISCLAIMER));

  GUI.drawList(renderer, Rect{0, listTop, pageWidth, listHeight}, static_cast<int>(cipher::kAlgorithmCount),
               static_cast<int>(algorithmIndex),
               [](const int i) -> std::string { return I18N.get(kAlgorithmTitles[i]); });

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CIPHER_RUN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void CipherActivity::renderResult(const Rect& header) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int x = metrics.contentSidePadding;
  int y = header.y + header.height + metrics.verticalSpacing;
  const int lineHeight = renderer.getLineHeight(SMALL_FONT_ID) + 6;

  renderer.drawText(SMALL_FONT_ID, x, y, I18N.get(kAlgorithmTitles[algorithmIndex]));
  y += lineHeight * 2;

  renderer.drawText(SMALL_FONT_ID, x, y, tr(STR_CIPHER_INPUT_LABEL));
  y += lineHeight;
  renderer.drawText(UI_10_FONT_ID, x, y, inputText);
  y += lineHeight * 2;

  renderer.drawText(SMALL_FONT_ID, x, y, tr(STR_CIPHER_RESULT_LABEL));
  y += lineHeight;
  renderer.drawText(UI_10_FONT_ID, x, y, resultOverflowed ? tr(STR_CIPHER_INVALID) : resultText);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CIPHER_NEW), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void CipherActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_CIPHER), false);
  } else {
    GUI.drawHeader(renderer, header, tr(STR_APP_CIPHER));
  }

  if (state == State::SelectAlgorithm) {
    renderSelect(header);
  } else {
    renderResult(header);
  }

  renderer.displayBuffer();
}
