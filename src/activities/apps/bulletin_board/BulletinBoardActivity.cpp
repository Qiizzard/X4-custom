#include "BulletinBoardActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <RadioManager.h>
#include <SecureStore.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "activities/apps/secure_vault/SecretEntryActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"
namespace {
constexpr char owner[] = "bulletin_board";
}
void BulletinBoardActivity::stop() {
  finalCount = server.count();
  server.stop();
  if (running) RADIO.shutdown(owner);
  running = false;
  securestore::secureZero(password, sizeof(password));
}
void BulletinBoardActivity::onExit() {
  stop();
  Activity::onExit();
}
void BulletinBoardActivity::start() {
  auto child = makeUniqueNoThrow<SecretEntryActivity>(renderer, mappedInput, StrId::STR_BOARD_PASSWORD, password,
                                                      sizeof(password), 8);
  if (!child) {
    LOG_ERR("Board", "Password input allocation failed");
    failed = true;
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(child), [this](const ActivityResult& result) {
    RenderLock lock;
    if (result.isCancelled) {
      securestore::secureZero(password, sizeof(password));
      return;
    }
    failed = false;
    ended = false;
    if (!RADIO.acquireAccessPoint(owner, "X4-Board", password, 1, 2))
      failed = true;
    else {
      running = true;
      if (!RADIO.accessPointAddress(owner, address) || !server.start(owner)) {
        failed = true;
        stop();
      } else
        started = millis();
    }
    securestore::secureZero(password, sizeof(password));
    requestUpdate();
  });
}
void BulletinBoardActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
      TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }
  RenderLock lock(*this);
  if (!running) {
    bool changed = false;
    if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
      duration = (duration + 2) % 3;
      changed = true;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      duration = (duration + 1) % 3;
      changed = true;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) start();
    if (changed) requestUpdate();
    return;
  }
  if (uint32_t(millis() - started) >= minutes[duration] * 60000u) {
    stop();
    ended = true;
    requestUpdate();
    return;
  }
  server.poll();
  if (uint32_t(millis() - refreshed) >= 1000) {
    refreshed = millis();
    requestUpdate();
  }
}
void BulletinBoardActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  GUI.drawHeader(renderer, header, tr(STR_BOARD_APP));
  const auto area = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  int y = area.y + line;
  auto draw = [&](const char* t) {
    UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID, y, t);
    y += line;
  };
  char text[128];
  draw(tr(STR_BOARD_SCOPE));
  draw(tr(STR_BOARD_SESSION));
  if (running) {
    draw(tr(STR_BOARD_JOIN));
    snprintf(text, sizeof(text), tr(STR_BOARD_URL), address[0], address[1], address[2], address[3]);
    draw(text);
    const unsigned elapsed = uint32_t(millis() - started) / 1000;
    const unsigned remaining = elapsed >= minutes[duration] * 60 ? 0 : minutes[duration] * 60 - elapsed;
    snprintf(text, sizeof(text), tr(STR_BOARD_STATUS), server.count(), remaining);
    draw(text);
  } else {
    snprintf(text, sizeof(text), tr(STR_BOARD_DURATION), minutes[duration]);
    draw(text);
    draw(tr(STR_BOARD_START));
    if (ended) {
      snprintf(text, sizeof(text), tr(STR_BOARD_ENDED), finalCount);
      draw(text);
    }
  }
  if (failed) draw(tr(STR_BOARD_FAILURE));
  draw(tr(STR_BOARD_CLIENT_LIMIT));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
