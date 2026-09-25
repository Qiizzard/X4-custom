#include "TaskManagerActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <RadioManager.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"
void TaskManagerActivity::refresh() {
  memory = MemoryStats::read(page == 1 ? MemoryStats::Pool::Psram : MemoryStats::Pool::Internal);
  sdReady = Storage.ready();
  radioHeld = RADIO.isHeld();
  snprintf(owner, sizeof(owner), "%s", RADIO.owner());
  heldMs = RADIO.heldForMs();
  refreshed = millis();
  requestUpdate();
}
void TaskManagerActivity::onEnter() {
  Activity::onEnter();
  refresh();
}
void TaskManagerActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  RenderLock lock(*this);
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    page = (page + 1) % 3;
    refresh();
  } else if (millis() - refreshed >= 5000)
    refresh();
}
void TaskManagerActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_TASK_MANAGER), false);
  else
    GUI.drawHeader(renderer, header, tr(STR_APP_TASK_MANAGER));
  const Rect area = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  int y = area.y + line;
  auto draw = [&](const char* text) {
    UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID, y, text);
    y += line;
  };
  char text[112];
  if (page < 2) {
    draw(page ? tr(STR_STATS_PSRAM) : tr(STR_STATS_INTERNAL));
    if (!memory.available)
      draw(tr(STR_STATS_UNAVAILABLE));
    else {
      snprintf(text, sizeof(text), tr(STR_STATS_TOTAL), static_cast<unsigned long>(memory.total));
      draw(text);
      snprintf(text, sizeof(text), tr(STR_STATS_FREE), static_cast<unsigned long>(memory.free));
      draw(text);
      snprintf(text, sizeof(text), tr(STR_STATS_LARGEST), static_cast<unsigned long>(memory.largest));
      draw(text);
      snprintf(text, sizeof(text), tr(STR_STATS_LOW), static_cast<unsigned long>(memory.lowWater));
      draw(text);
      draw(tr(STR_STATS_SNAPSHOT));
    }
  } else {
    draw(sdReady ? tr(STR_STATS_SD_READY) : tr(STR_STATS_SD_MISSING));
    draw(radioHeld ? tr(STR_STATS_RADIO_HELD) : tr(STR_STATS_RADIO_IDLE));
    if (radioHeld) {
      draw(owner);
      snprintf(text, sizeof(text), tr(STR_STATS_HOLD), static_cast<unsigned long>(heldMs));
      draw(text);
    }
    snprintf(text, sizeof(text), tr(STR_STATS_SCREEN), renderer.getScreenWidth(), renderer.getScreenHeight());
    draw(text);
    snprintf(text, sizeof(text), tr(STR_STATS_TICKS), static_cast<unsigned long>(refreshed));
    draw(text);
  }
  draw(tr(STR_STATS_LIMIT));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_STATS_NEXT), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
