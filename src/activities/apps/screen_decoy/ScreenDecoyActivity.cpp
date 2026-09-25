#include "ScreenDecoyActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"
void ScreenDecoyActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
      (state != State::Active && TouchHeaderBackButton::wasTapped(mappedInput, renderer))) {
    if (state == State::Select) {
      finish();
      return;
    }
    RenderLock lock(*this);
    state = State::Select;
    requestUpdate();
    return;
  }
  RenderLock lock(*this);
  if (state == State::Select) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      selected = (selected + 1) % 4;
      requestUpdate();
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
      selected = (selected + 3) % 4;
      requestUpdate();
    }
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (state == State::Select)
      state = State::Preview;
    else if (state == State::Preview)
      state = State::Active;
    else
      state = State::Select;  // Also permits leaving a blank screen on mapped Confirm.
    requestUpdate();
  }
}
void ScreenDecoyActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect area = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  int y = area.y + line;
  auto draw = [&](const char* text) {
    UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID, y, text);
    y += line;
  };
  static constexpr StrId names[] = {StrId::STR_DECOY_SHUTDOWN, StrId::STR_DECOY_ERROR, StrId::STR_DECOY_READING,
                                    StrId::STR_DECOY_BLANK};
  if (state == State::Select) {
    draw(I18N.get(names[selected]));
    draw(tr(STR_DECOY_WARNING));
    draw(tr(STR_DECOY_SLEEP));
    draw(tr(STR_DECOY_EXIT));
  } else if (selected == 0) {
    draw(tr(STR_DECOY_BATTERY));
    draw(tr(STR_DECOY_CHARGE));
  } else if (selected == 1) {
    draw(tr(STR_DECOY_SD_ERROR));
    draw(tr(STR_DECOY_SD_MESSAGE));
  } else if (selected == 2) {
    draw(tr(STR_DECOY_BOOK_TITLE));
    y += line;
    draw(tr(STR_DECOY_BOOK_1));
    draw(tr(STR_DECOY_BOOK_2));
    draw(tr(STR_DECOY_BOOK_3));
  }
  if (state != State::Active) {
    const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
    if (mappedInput.hasTouchHardware())
      TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_SCREEN_DECOY), false);
    else
      GUI.drawHeader(renderer, header, tr(STR_APP_SCREEN_DECOY));
    if (state == State::Preview)
      UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID, area.y + area.height - 2 * line, tr(STR_DECOY_PREVIEW));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  renderer.displayBuffer();
}
