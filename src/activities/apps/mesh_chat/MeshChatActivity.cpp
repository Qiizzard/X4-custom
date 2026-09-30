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
void MeshChatActivity::observePeer(const uint8_t* bytes) {
  unsigned i = 0;
  while (i < peerCount && memcmp(peers[i].mac, bytes + 1, 6)) ++i;
  if (i == peerCount) {
    if (peerCount == 16) return;
    ++peerCount;
    memcpy(peers[i].mac, bytes + 1, 6);
  }
  memset(peers[i].name, 0, sizeof(peers[i].name));
  for (unsigned j = 0; j < 16 && bytes[7 + j]; ++j)
    peers[i].name[j] = bytes[7 + j] >= 32 && bytes[7 + j] <= 126 ? bytes[7 + j] : '?';
  peers[i].seen = millis();
  dirty = true;
}
bool MeshChatActivity::remember(const uint8_t* bytes) {
  uint32_t hash = 2166136261u;
  // Full sender/name/text bytes, excluding hop count; not an authenticity check.
  for (unsigned i = 0; i < 223; ++i) hash = (hash ^ bytes[i]) * 16777619u;
  const uint32_t now = millis();
  for (unsigned i = 0; i < hashCount; ++i)
    if (hashes[i] == hash && uint32_t(now - hashTimes[i]) < 30000) return false;
  hashes[hashNext] = hash;
  hashTimes[hashNext] = now;
  hashNext = (hashNext + 1) % 16;
  if (hashCount < 16) ++hashCount;
  return true;
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
    if (sent) {
      remember(packet);
      append(packet);
    }
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
  const uint32_t now = millis();
  for (unsigned i = 0; i < peerCount;) {
    if (uint32_t(now - peers[i].seen) >= 90000) {
      for (unsigned j = i + 1; j < peerCount; ++j) peers[j - 1] = peers[j];
      --peerCount;
      if (peerSelected >= peerCount) peerSelected = 0;
      dirty = true;
    } else
      ++i;
  }
  for (unsigned i = 0; i < 4; ++i) {
    const auto length = incoming.pop(packet, sizeof(packet));
    if (!length) break;
    const bool chat = length == sizeof(packet) && packet[0] == 1 && packet[223] <= 3;
    const bool presence = length == 23 && packet[0] == 2;
    if ((!chat && !presence) || !memcmp(packet + 1, mac, 6)) continue;
    observePeer(packet);
    if (chat && remember(packet)) {
      append(packet);
      if (relay && packet[223] < 3) {
        ++packet[223];
        relayQueue.push(packet, sizeof(packet));
      }
    }
  }
  if (uint32_t(now - lastPresence) >= 10000) {
    memset(packet, 0, sizeof(packet));
    packet[0] = 2;
    memcpy(packet + 1, mac, 6);
    memcpy(packet + 7, "CrossInk", 8);
    if (!RADIO.sendEspNowBroadcast(owner, packet, 23)) {
      failed = true;
      dirty = true;
    }
    lastPresence = now;
  }
  if (relay && uint32_t(now - lastRelay) >= 1000 && relayQueue.pop(packet, sizeof(packet))) {
    if (!RADIO.sendEspNowBroadcast(owner, packet, sizeof(packet))) failed = true;
    lastRelay = now;
    dirty = true;
  }
  const bool changedView = mappedInput.wasPressed(MappedInputManager::Button::PageBack);
  if (changedView) {
    view = (view + 1) % 3;
    dirty = true;
  }
  if (confirm && !changedView) {
    if (view == 0)
      compose();
    else if (view == 2) {
      relay = !relay;
      relayQueue.reset();
      lastRelay = now;
      dirty = true;
    }
  }
  const bool left = mappedInput.wasPressed(MappedInputManager::Button::Left);
  const bool right = mappedInput.wasPressed(MappedInputManager::Button::Right);
  if (view == 0) {
    if (count && left) {
      selected = (selected + count - 1) % count;
      page = 0;
      dirty = true;
    }
    if (count && right) {
      selected = (selected + 1) % count;
      page = 0;
      dirty = true;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::PageForward)) {
      page = (page + 1) % 4;
      dirty = true;
    }
  } else if (view == 1 && peerCount) {
    if (left) {
      peerSelected = (peerSelected + peerCount - 1) % peerCount;
      dirty = true;
    }
    if (right) {
      peerSelected = (peerSelected + 1) % peerCount;
      dirty = true;
    }
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
    if (view == 0 && count) {
      draw(messages[selected].name);
      char part[51]{};
      memcpy(part, messages[selected].text + page * 50, page == 3 ? 49 : 50);
      draw(part);
      snprintf(text, sizeof(text), tr(STR_MESH_PAGE), page + 1);
      draw(text);
    }
    if (view == 1) {
      snprintf(text, sizeof(text), tr(STR_MESH_PEERS), peerCount);
      draw(text);
      if (peerCount) {
        draw(peers[peerSelected].name);
        const auto* address = peers[peerSelected].mac;
        snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X", address[0], address[1], address[2], address[3],
                 address[4], address[5]);
        draw(text);
      }
    } else if (view == 2) {
      draw(relay ? tr(STR_MESH_RELAY_ON) : tr(STR_MESH_RELAY_OFF));
      snprintf(text, sizeof(text), tr(STR_MESH_RELAY_DROPS), static_cast<unsigned long>(relayQueue.droppedFrames()));
      draw(text);
      draw(tr(STR_MESH_RELAY_LIMIT));
    }
    draw(view == 0 ? tr(STR_MESH_CONTROLS) : tr(STR_MESH_VIEW_CONTROLS));
    draw(tr(STR_MESH_VIEWS));
    if (sent) draw(tr(STR_MESH_QUEUED));
  }
  if (failed) draw(tr(STR_MESH_ERROR));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
