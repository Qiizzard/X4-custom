#include "SecretEntryActivity.h"

#include <Arduino.h>
#include <I18n.h>
#include <SecureStore.h>

#include <cstring>

#include "components/UITheme.h"
#include "fontIds.h"

SecretEntryActivity::~SecretEntryActivity() { securestore::secureZero(draft, sizeof(draft)); }
void SecretEntryActivity::cancel() {
  result.isCancelled = true;
  securestore::secureZero(draft, sizeof(draft));
  finish();
}
void SecretEntryActivity::onEnter() {
  Activity::onEnter();
  if (!destination || capacity < 2 || capacity > sizeof(draft) || minimum >= capacity) {
    LOG_ERR("SecretEntry", "Invalid destination bounds");
    cancel();
    return;
  }
  // Do not prefill: callers use a separate entry buffer, not the active key.
  securestore::secureZero(destination, capacity);
  securestore::secureZero(draft, sizeof(draft));
  length = 0;
  ready = true;
  lastInput = millis();
  requestUpdate();
}
void SecretEntryActivity::onExit() {
  securestore::secureZero(draft, sizeof(draft));
  length = 0;
  ready = false;
  Activity::onExit();
}
void SecretEntryActivity::loop() {
  if (!ready) return;
  if (millis() - lastInput >= 60000) {
    cancel();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    cancel();
    return;
  }
  bool changed = false;
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    candidate = candidate == 126 ? 32 : candidate + 1;
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    candidate = candidate == 32 ? 126 : candidate - 1;
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
    candidate = 32 + (candidate - 32 + 10) % 95;
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    if (length) securestore::secureZero(draft + --length, 1);
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    if (length + 1 < capacity) {
      draft[length++] = candidate;
      draft[length] = 0;
    }
    changed = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    changed = true;
    if (length >= minimum) {
      memcpy(destination, draft, length + 1);
      securestore::secureZero(draft, sizeof(draft));
      result.isCancelled = false;
      finish();
      return;
    }
  }
  if (changed) {
    lastInput = millis();
    requestUpdate();
  }
}
void SecretEntryActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& m = UITheme::getInstance().getMetrics();
  int mt, mr, mb, ml;
  renderer.getOrientedViewableTRBL(&mt, &mr, &mb, &ml);
  GUI.drawHeader(renderer, Rect{ml, mt + m.topPadding, renderer.getScreenWidth() - ml - mr, m.headerHeight},
                 I18N.get(prompt));
  const int top = mt + m.topPadding + m.headerHeight + 30;
  char masked[65];
  memset(masked, '*', length);
  masked[length] = 0;
  renderer.drawCenteredText(UI_10_FONT_ID, top, masked);
  char selection[4] = {'[', candidate, ']', 0};
  renderer.drawCenteredText(UI_12_FONT_ID, top + 45, selection);
  renderer.drawCenteredText(SMALL_FONT_ID, top + 95, tr(STR_SECRET_CHOOSE));
  renderer.drawCenteredText(SMALL_FONT_ID, top + 120, tr(STR_SECRET_EDIT));
  renderer.drawCenteredText(SMALL_FONT_ID, top + 145, tr(STR_SECRET_JUMP));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
