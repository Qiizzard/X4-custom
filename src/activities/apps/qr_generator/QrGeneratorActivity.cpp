// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
#include "QrGeneratorActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Memory.h>

#include <cstdio>
#include <memory>
#include <string>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/QrUtils.h"

void QrGeneratorActivity::onEnter() {
  Activity::onEnter();
  state = State::TextInput;
  payload[0] = '\0';

  auto keyboard =
      makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_QR_ENTER_TEXT), "", kMaxPayload);
  if (!keyboard) {
    LOG_ERR("QR", "Cannot allocate keyboard (%u bytes)", static_cast<unsigned>(sizeof(KeyboardEntryActivity)));
    finish();
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    const auto* keyboard = std::get_if<KeyboardResult>(&result.data);
    if (result.isCancelled || keyboard == nullptr || keyboard->text.empty()) {
      finish();
      return;
    }
    snprintf(payload, sizeof(payload), "%s", keyboard->text.c_str());
    state = State::QrDisplay;
    requestUpdate();
  });
}

void QrGeneratorActivity::onExit() { Activity::onExit(); }

void QrGeneratorActivity::loop() {
  if (state != State::QrDisplay) return;

  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_QR_ENTER_TEXT),
                                                             std::string(payload), kMaxPayload);
    if (!keyboard) {
      LOG_ERR("QR", "Cannot allocate keyboard (%u bytes)", static_cast<unsigned>(sizeof(KeyboardEntryActivity)));
      return;
    }
    startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
      const auto* keyboard = std::get_if<KeyboardResult>(&result.data);
      if (result.isCancelled || keyboard == nullptr || keyboard->text.empty()) {
        state = State::QrDisplay;
        requestUpdate();
        return;
      }
      snprintf(payload, sizeof(payload), "%s", keyboard->text.c_str());
      state = State::QrDisplay;
      requestUpdate();
    });
  }
}

void QrGeneratorActivity::render(RenderLock&&) {
  renderer.clearScreen();
  if (state != State::QrDisplay) {
    renderer.displayBuffer();
    return;
  }

  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_QR_GENERATOR), false);
  } else {
    GUI.drawHeader(renderer, header, tr(STR_APP_QR_GENERATOR));
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int startY = header.y + header.height + metrics.verticalSpacing;
  const Rect qrBounds{metrics.contentSidePadding, startY, pageWidth - metrics.contentSidePadding * 2,
                      pageHeight - startY - metrics.buttonHintsHeight};

  // Bridging the fixed buffer into QrUtils' std::string-taking API -- the
  // same pattern other ported apps use to reach a std::string-based shared
  // component from a char[] field (rules 1 and 3 govern this app's own
  // state, not a one-shot render-time construction into a shared utility).
  QrUtils::drawQrCode(renderer, qrBounds, std::string(payload));

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_QR_NEW), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
}
