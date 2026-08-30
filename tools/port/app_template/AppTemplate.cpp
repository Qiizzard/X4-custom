#include "{{CLASS}}Activity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>  // makeUniqueNoThrow<T[]>() -- never bare `new` (rule 6)

#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"  // defines GUI; rule 19 says draw through it
#include "fontIds.h"

static constexpr const char* TAG = "{{SLUG}}";

void {{CLASS}}Activity::onEnter() {
  Activity::onEnter();
  // RULESET rule 14: allocate long-lived buffers here with makeUniqueNoThrow<T[]>(n),
  // check for nullptr, and LOG_ERR + fall back on OOM.
  //
  // If (and only if) this app needs the radio, take it here and give it back in
  // onExit() -- rules 7 and 8. Never touch WiFi/BLE outside RadioManager:
  //   if (!RADIO.acquire(RadioManager::Mode::WifiScan, "{{SLUG}}")) {
  //     LOG_ERR(TAG, "radio unavailable");
  //     ...show a busy state, do not retry in a loop...
  //   }
  dirty = true;
  requestUpdate();
}

void {{CLASS}}Activity::onExit() {
  // RULESET rule 14: free in reverse order of acquisition, delete every
  // FreeRTOS task, and close every file handle. If you acquired the radio,
  // RADIO.shutdown() belongs here unconditionally -- leaving it held is the
  // state leak rule 8 exists to prevent.
  Activity::onExit();
}

void {{CLASS}}Activity::loop() {
  // All real work happens here. A radio callback or ISR may only copy raw bytes
  // into a pre-allocated RingBuffer (rule 10) -- drain it from this function.
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  // TODO: handle navigation (Button::Up / Button::Down) and selection
  // (Button::Confirm). Rule 20: logical buttons only, never raw indices.

  if (dirty) {
    dirty = false;
    requestUpdate();
  }
}

void {{CLASS}}Activity::render(RenderLock&&) {
  // Rule 19: never hardcode 800/480 -- ask the renderer.
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  // headerRect() gives the touch-aware header bounds; the branch below keeps a
  // tappable back button on touch devices and a plain header everywhere else.
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  // Rule 18: every user-facing string goes through tr(STR_*). Add STR_APP_{{UPPER}}
  // to lib/I18n/translations/*.yaml and regenerate with scripts/gen_i18n.py.
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_{{UPPER}}), false);
  } else {
    GUI.drawHeader(renderer, header, tr(STR_APP_{{UPPER}}));
  }

  // TODO: draw this screen's content through UITheme/GUI so it matches every
  // other app -- GUI.drawList(...), renderer.drawCenteredText(UI_10_FONT_ID, ...).
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, "TODO", true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
