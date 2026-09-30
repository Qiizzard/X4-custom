#include "VaultDeleteActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

void VaultDeleteActivity::removeVaultFiles() {
  // Exact app-owned filenames only: no recursion, directory removal or caller-supplied paths.
  static constexpr const char* paths[] = {
      "/crossink/vaults/passwords.bin", "/crossink/vaults/passwords.next", "/crossink/vaults/passwords.previous",
      "/crossink/vaults/totp.bin",      "/crossink/vaults/totp.next",      "/crossink/vaults/totp.previous",
      "/crossink/vaults/decoy.bin",     "/crossink/vaults/decoy.next",     "/crossink/vaults/decoy.previous"};
  for (const auto* path : paths) {
    if (!Storage.ready()) {
      ++failed;
      LOG_ERR("VaultDelete", "SD unavailable");
    } else if (!Storage.exists(path)) {
      // exists() cannot distinguish absence from an I/O error; UI makes this explicit.
      ++notFound;
    } else if (Storage.remove(path)) {
      ++removed;
    } else {
      ++failed;
      LOG_ERR("VaultDelete", "Could not delete %s", path);
    }
  }
}
void VaultDeleteActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
      TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    if (state != State::Armed) {
      finish();
      return;
    }
    RenderLock lock(*this);
    state = State::Review;
    requestUpdate();
    return;
  }
  RenderLock lock(*this);
  if (state == State::Armed && millis() - armedAt >= 15000) {
    state = State::Review;
    requestUpdate();
    return;
  }
  if (state == State::Review && mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    state = State::Armed;
    armedAt = millis();
    requestUpdate();
  } else if (state == State::Armed && mappedInput.wasPressed(MappedInputManager::Button::PageForward)) {
    removeVaultFiles();
    state = State::Done;
    requestUpdate();
  }
}
void VaultDeleteActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::draw(renderer, header, tr(STR_VAULT_DELETE_APP), false);
  else
    GUI.drawHeader(renderer, header, tr(STR_VAULT_DELETE_APP));
  const Rect area = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  int y = area.y + line;
  auto draw = [&](const char* text) {
    UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID, y, text);
    y += line;
  };
  if (state == State::Done) {
    char text[96];
    snprintf(text, sizeof(text), tr(STR_VAULT_DELETE_RESULT), removed, failed, notFound);
    draw(text);
    draw(tr(STR_VAULT_DELETE_UNKNOWN));
  } else {
    draw(tr(STR_VAULT_DELETE_SCOPE));
    draw(tr(STR_VAULT_DELETE_BACKUPS));
    draw(tr(STR_VAULT_DELETE_PRESERVED));
    draw(state == State::Armed ? tr(STR_VAULT_DELETE_CONFIRM) : tr(STR_VAULT_DELETE_REVIEW));
  }
  draw(tr(STR_VAULT_DELETE_LIMIT));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
