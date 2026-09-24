#include "HttpClientActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <RadioManager.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"
namespace {
constexpr char kOwner[] = "http_client";
}
void HttpClientActivity::onEnter() {
  Activity::onEnter();
  owned = RADIO.acquire(RadioManager::Mode::WifiStation, kOwner);
  if (!owned) {
    state = State::Failed;
    requestUpdate();
    return;
  }
  auto picker = makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput, true, false, kOwner);
  if (!picker) {
    LOG_ERR("NHTTP", "Picker allocation failed");
    state = State::Failed;
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(picker), [this](const ActivityResult& result) {
    if (result.isCancelled) {
      finish();
      return;
    }
    state = State::Menu;
    requestUpdate();
  });
}
void HttpClientActivity::onExit() {
  Activity::onExit();
  if (owned && RADIO.shutdown(kOwner)) owned = false;
}
void HttpClientActivity::edit(bool bodyField) {
  auto keyboard =
      makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, bodyField ? tr(STR_HTTP_BODY) : tr(STR_HTTP_URL),
                                               bodyField ? body : url, bodyField ? 512 : 256);
  if (!keyboard) {
    LOG_ERR("NHTTP", "Keyboard allocation failed");
    state = State::Failed;
    requestUpdate();
    return;
  }
  state = State::Waiting;
  startActivityForResult(std::move(keyboard), [this, bodyField](const ActivityResult& result) {
    if (!result.isCancelled) {
      const auto& input = std::get<KeyboardResult>(result.data).text;
      const size_t capacity = bodyField ? sizeof(body) : sizeof(url);
      if (input.size() >= capacity) {
        LOG_ERR("NHTTP", "Input exceeds capacity");
        state = State::Failed;
        requestUpdate();
        return;
      }
      memcpy(bodyField ? body : url, input.c_str(), input.size() + 1);
    }
    state = State::Menu;
    requestUpdate();
  });
}
void HttpClientActivity::send() {
  {
    RenderLock lock(*this);
    state = State::Working;
    offset = 0;
  }
  if (requestUpdateAndWait() != RequestUpdateResult::Rendered) {
    LOG_ERR("NHTTP", "Request screen unavailable");
    RenderLock lock(*this);
    state = State::Failed;
    requestUpdate();
    return;
  }
  // Render does not inspect response while Working; no whole-response allocation.
  const bool ok = NetworkToolRequest::send(kOwner, url, post ? body : nullptr, response);
  {
    RenderLock lock(*this);
    state = ok ? State::Result : State::Failed;
  }
  requestUpdate();
}
void HttpClientActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (owned && (state == State::Result || state == State::Failed)) {
      RenderLock lock(*this);
      state = State::Menu;
      requestUpdate();
    } else
      finish();
    return;
  }
  const bool next = mappedInput.wasPressed(MappedInputManager::Button::Right) ||
                    mappedInput.wasPressed(MappedInputManager::Button::Down) ||
                    mappedInput.wasPressed(MappedInputManager::Button::PageForward);
  const bool previous = mappedInput.wasPressed(MappedInputManager::Button::Left) ||
                        mappedInput.wasPressed(MappedInputManager::Button::Up) ||
                        mappedInput.wasPressed(MappedInputManager::Button::PageBack);
  if (next || previous) {
    RenderLock lock(*this);
    if (state == State::Menu)
      selected = (selected + (next ? 1 : 3)) % 4;
    else if (state == State::Result) {
      if (next && offset + pageBytes < response.bytes)
        offset += pageBytes;
      else if (previous)
        offset = offset >= pageBytes ? offset - pageBytes : 0;
    }
    requestUpdate();
  }
  if (state != State::Menu || !mappedInput.wasPressed(MappedInputManager::Button::Confirm)) return;
  if (selected == 0) {
    RenderLock lock(*this);
    post = !post;
    requestUpdate();
  } else if (selected == 1)
    edit(false);
  else if (selected == 2)
    edit(true);
  else
    send();
}
void HttpClientActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_HTTP_CLIENT), false);
  else
    GUI.drawHeader(renderer, header, tr(STR_APP_HTTP_CLIENT));
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  int y = screen.y + line;
  char text[112];
  auto draw = [&](const char* value) {
    UITheme::drawCenteredText(renderer, screen, UI_10_FONT_ID, y, value);
    y += line;
  };
  if (state == State::Menu) {
    snprintf(text, sizeof(text), "%s", post ? "POST" : "GET");
    draw(text);
    snprintf(text, sizeof(text), "%.40s", url);
    draw(text);
    const char* label = selected == 0   ? tr(STR_HTTP_METHOD)
                        : selected == 1 ? tr(STR_HTTP_URL)
                        : selected == 2 ? tr(STR_HTTP_BODY)
                                        : tr(STR_HTTP_SEND);
    draw(label);
    draw(tr(STR_HTTP_MENU_HINT));
    if (selected == 2) draw(post ? tr(STR_HTTP_POST_TEXT) : tr(STR_HTTP_BODY_UNUSED));
  } else if (state == State::Result) {
    snprintf(text, sizeof(text), tr(STR_HTTP_STATUS), response.status, static_cast<unsigned long>(response.elapsedMs));
    draw(text);
    draw(response.truncated ? tr(STR_HTTP_TRUNCATED) : tr(STR_HTTP_PREVIEW));
    const int columns =
        std::max(1, std::min(48, (screen.width - 24) / std::max(1, renderer.getTextWidth(UI_10_FONT_ID, "W"))));
    const int rows = std::max(1, (screen.height - 5 * line) / line);
    pageBytes = size_t(columns) * rows;
    // Flattened ASCII preview: binary/non-ASCII bytes are replaced in the helper.
    for (int row = 0; row < rows && offset + size_t(row * columns) < response.bytes; ++row) {
      const size_t begin = offset + size_t(row * columns);
      const size_t n = std::min(size_t(columns), response.bytes - begin);
      memcpy(text, response.body + begin, n);
      text[n] = 0;
      draw(text);
    }
  } else
    draw(state == State::Working  ? tr(STR_HTTP_WORKING)
         : state == State::Failed ? tr(STR_HTTP_FAILED)
                                  : tr(STR_LOADING));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), state == State::Menu ? tr(STR_CONFIRM) : "", tr(STR_DIR_LEFT),
                                            tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
