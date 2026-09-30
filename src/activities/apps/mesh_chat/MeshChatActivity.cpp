#include "MeshChatActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <RadioManager.h>

#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"
namespace {
constexpr char owner[] = "mesh_chat";
}
void MeshChatActivity::receive(void* context, const uint8_t* bytes, uint16_t length) {
  // WiFi task: bounded queue copy only. Parsing/rendering stays in loop().
  static_cast<MeshChatActivity*>(context)->incoming.push(bytes, length);
}
void MeshChatActivity::onExit() {
  if (running) RADIO.shutdown(owner);  // Detaches and waits out callback copy before destruction.
  running = false;
  Activity::onExit();
}
void MeshChatActivity::append(const uint8_t* bytes) {
  auto& m = messages[next];
  memset(&m, 0, sizeof(m));
  for (unsigned i = 0; i < 16 && bytes[7 + i]; ++i)
    m.name[i] = bytes[7 + i] >= 32 && bytes[7 + i] <= 126 ? bytes[7 + i] : '?';
  for (unsigned i = 0; i < 199 && bytes[23 + i]; ++i)
    m.text[i] = bytes[23 + i] >= 32 && bytes[23 + i] <= 126 ? bytes[23 + i] : '?';
  selected = next;
  page = 0;
  next = (next + 1) % 8;
  if (count < 8) ++count;
  dirty = true;
}
void MeshChatActivity::compose() {
  auto child = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_MESH_COMPOSE), "", 64);
  if (!child) {
    LOG_ERR("MeshChat", "Input allocation failed");
    failed = true;
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(child), [this](const ActivityResult& result) {
    RenderLock lock;
    if (result.isCancelled) return;
    const auto* text = std::get_if<KeyboardResult>(&result.data);
    if (!text || text->text.empty() || text->text.size() > 64) return;
    memset(packet, 0, sizeof(packet));
    packet[0] = 1;
    memcpy(packet + 1, mac, 6);
    memcpy(packet + 7, "CrossInk", 8);
    memcpy(packet + 23, text->text.data(), text->text.size());
    sent = RADIO.sendEspNowBroadcast(owner, packet, sizeof(packet));
    failed = !sent;
    if (sent) append(packet);
    requestUpdate();
  });
}
void MeshChatActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
      TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }
  RenderLock lock(*this);
  const bool confirm = mappedInput.wasPressed(MappedInputManager::Button::Confirm);
  if (!running) {
    if (confirm) {
      if (!RADIO.acquire(RadioManager::Mode::EspNow, owner))
        failed = true;
      else if (!RadioManager::stationMac(mac) || !RADIO.startEspNowBroadcast(owner, 1, receive, this)) {
        RADIO.shutdown(owner);
        failed = true;
      } else {
        running = true;
        failed = false;
      }
      requestUpdate();
    }
    return;
  }
  for (unsigned i = 0; i < 4; ++i) {
    const auto length = incoming.pop(packet, sizeof(packet));
    if (!length) break;
    if (length == sizeof(packet) && packet[0] == 1 && memcmp(packet + 1, mac, 6)) append(packet);
  }
  if (confirm) compose();
  if (count && mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    selected = (selected + count - 1) % count;
    page = 0;
    dirty = true;
  }
  if (count && mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    selected = (selected + 1) % count;
    page = 0;
    dirty = true;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::PageForward)) {
    page = (page + 1) % 4;
    dirty = true;
  }
  if (dirty && millis() - lastRender >= 1000) {
    lastRender = millis();
    dirty = false;
    requestUpdate();
  }
}
void MeshChatActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  GUI.drawHeader(renderer, header, tr(STR_MESH_APP));
  const auto area = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  int y = area.y + line;
  auto draw = [&](const char* t) {
    UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID, y, t);
    y += line;
  };
  draw(tr(STR_MESH_WARNING));
  draw(tr(STR_MESH_LIMIT));
  if (!running)
    draw(tr(STR_MESH_START));
  else {
    char text[96];
    snprintf(text, sizeof(text), tr(STR_MESH_STATUS), count, static_cast<unsigned long>(incoming.droppedFrames()));
    draw(text);
    if (count) {
      draw(messages[selected].name);
      char part[51]{};
      memcpy(part, messages[selected].text + page * 50, page == 3 ? 49 : 50);
      draw(part);
      snprintf(text, sizeof(text), tr(STR_MESH_PAGE), page + 1);
      draw(text);
    }
    draw(tr(STR_MESH_CONTROLS));
    if (sent) draw(tr(STR_MESH_QUEUED));
  }
  if (failed) draw(tr(STR_MESH_ERROR));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
