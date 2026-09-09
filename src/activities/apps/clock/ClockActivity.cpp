#include "ClockActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <cstring>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
char dateSeparatorChar() {
  switch (SETTINGS.dateSeparator) {
    case CrossPointSettings::DATE_SEPARATOR_PERIOD:
      return '.';
    case CrossPointSettings::DATE_SEPARATOR_HYPHEN:
      return '-';
    case CrossPointSettings::DATE_SEPARATOR_SLASH:
    default:
      return '/';
  }
}
}  // namespace

void ClockActivity::onEnter() {
  Activity::onEnter();
  lastTimeText[0] = '\0';
  lastDateText[0] = '\0';
  lastAvailable = false;
  refresh();
  requestUpdate();
}

void ClockActivity::onExit() { Activity::onExit(); }

bool ClockActivity::refresh() {
  available = halClock.isAvailable();

  if (available &&
      halClock.formatTime(timeText, sizeof(timeText), SETTINGS.clockUtcOffsetQ, SETTINGS.clockFormat == 1)) {
    // formatted fine
  } else {
    timeText[0] = '\0';
    available = false;
  }

  // Mirror HeaderDate's caution: don't draw a date until the RTC's date has
  // actually been confirmed synced, rather than showing a plausible-looking
  // but possibly-wrong calendar date (rule 21).
  if (available && SETTINGS.clockDateHasBeenSynced &&
      halClock.formatDate(dateText, sizeof(dateText), SETTINGS.clockUtcOffsetQ,
                          static_cast<HalClock::DateFormat>(SETTINGS.dateFormat), dateSeparatorChar())) {
    // formatted fine
  } else {
    dateText[0] = '\0';
  }

  const bool changed = available != lastAvailable || std::strcmp(timeText, lastTimeText) != 0 ||
                       std::strcmp(dateText, lastDateText) != 0;
  if (changed) {
    lastAvailable = available;
    std::strncpy(lastTimeText, timeText, sizeof(lastTimeText) - 1);
    lastTimeText[sizeof(lastTimeText) - 1] = '\0';
    std::strncpy(lastDateText, dateText, sizeof(lastDateText) - 1);
    lastDateText[sizeof(lastDateText) - 1] = '\0';
  }
  return changed;
}

void ClockActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (refresh()) {
    requestUpdate();
  }
}

void ClockActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_CLOCK), false);
  } else {
    GUI.drawHeader(renderer, header, tr(STR_APP_CLOCK));
  }

  if (!available) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_CLOCK_RTC_UNAVAILABLE), true);
  } else {
    renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2 - 10, timeText, true, EpdFontFamily::BOLD);
    if (dateText[0] != '\0') {
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 30, dateText, true);
    } else {
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 30, tr(STR_CLOCK_DATE_PENDING), true);
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
  (void)pageWidth;
}
