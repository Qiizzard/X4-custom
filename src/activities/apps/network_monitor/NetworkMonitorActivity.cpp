#include "NetworkMonitorActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include "MappedInputManager.h"
#include "activities/apps/passive_monitor/PassiveMonitorActivity.h"
#include "activities/apps/wifi_scanner/WifiScannerActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"
void NetworkMonitorActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Left) ||
      mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    RenderLock lock(*this);
    groups = !groups;
    failed = false;
    requestUpdate();
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    std::unique_ptr<Activity> child;
    if (groups)
      child = makeUniqueNoThrow<WifiScannerActivity>(renderer, mappedInput, true);
    else
      child = makeUniqueNoThrow<PassiveMonitorActivity>(renderer, mappedInput, PassiveMonitorActivity::Kind::Deauth);
    if (!child) {
      LOG_ERR("NETMON", "Child allocation failed");
      RenderLock lock(*this);
      failed = true;
      requestUpdate();
      return;
    }
    // Each child owns/releases its radio hold; this menu never acquires one.
    startActivityForResult(std::move(child), [this](const ActivityResult&) { requestUpdate(); });
  }
}
void NetworkMonitorActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_NETWORK_MONITOR), false);
  else
    GUI.drawHeader(renderer, header, tr(STR_APP_NETWORK_MONITOR));
  const Rect area = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID, area.y + line,
                            groups ? tr(STR_NET_GROUP_TITLE) : tr(STR_APP_DEAUTH_DETECTOR));
  UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID, area.y + 3 * line, tr(STR_NET_GROUP_LIMIT));
  if (failed) UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID, area.y + 4 * line, tr(STR_NET_MON_ALLOC));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
