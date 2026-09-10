// Adapted from biscuit, MIT, Copyright (c) 2025 Dave Allie.
#include "EventLoggerActivity.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>

#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

void EventLoggerActivity::onEnter() {
  Activity::onEnter();
  entries = makeUniqueNoThrow<Entry[]>(MAX_ENTRIES);
  if (!entries) {
    LOG_ERR("EventLogger", "Cannot allocate entry buffer");
    finish();
    return;
  }
  loadEntries();
  requestUpdate();
}
void EventLoggerActivity::onExit() {
  entries.reset();
  Activity::onExit();
}

void EventLoggerActivity::loadEntries() {
  count = next = selected = 0;
  storageError = false;
  if (!Storage.exists(LOG_PATH)) return;
  auto file = Storage.open(LOG_PATH);
  if (!file) {
    LOG_ERR("EventLogger", "Cannot open log");
    storageError = true;
    return;
  }
  if (file.fileSize() > MAX_FILE) {
    LOG_ERR("EventLogger", "Log exceeds bounded file size");
    storageError = true;
    file.close();
    return;
  }
  char line[144];
  size_t length = 0;
  while (file.available() && !storageError) {
    const int c = file.read();
    if (c < 0) {
      storageError = true;
      break;
    }
    if (c == '\n') {
      line[length] = 0;
      char* comma = strchr(line, ',');
      if (!comma || comma == line || !comma[1] || strlen(comma + 1) > 127) {
        storageError = true;
        break;
      }
      uint32_t timestamp = 0;
      for (const char* p = line; p < comma; ++p) {
        if (*p < '0' || *p > '9' || timestamp > (std::numeric_limits<uint32_t>::max() - (*p - '0')) / 10) {
          storageError = true;
          break;
        }
        timestamp = timestamp * 10 + (*p - '0');
      }
      if (storageError) break;
      entries[next].uptime = timestamp;
      memcpy(entries[next].text, comma + 1, strlen(comma + 1) + 1);
      next = (next + 1) % MAX_ENTRIES;
      count = std::min(count + 1, MAX_ENTRIES);
      length = 0;
    } else {
      if (c == 0 || c == '\r' || length == sizeof(line) - 1) {
        storageError = true;
        break;
      }
      line[length++] = static_cast<char>(c);
    }
  }
  if (length != 0) storageError = true;  // never append onto a torn final record
  if (!file.close()) storageError = true;
  if (storageError) LOG_ERR("EventLogger", "Log read/format error; preserved file and disabled appends");
}

void EventLoggerActivity::saveEntry(const char* text) {
  if (storageError || !text || !text[0]) return;
  const size_t length = strlen(text);
  if (length > 127 || strchr(text, '\n') || strchr(text, '\r')) {
    LOG_ERR("EventLogger", "Rejected invalid note length or newline");
    storageError = true;
    return;
  }
  if (!Storage.exists("/crossink/logs") && !Storage.mkdir("/crossink/logs", true)) {
    LOG_ERR("EventLogger", "Cannot create log directory");
    storageError = true;
    return;
  }
  auto file = Storage.open(LOG_PATH, O_WRITE | O_CREAT | O_APPEND);
  if (!file) {
    LOG_ERR("EventLogger", "Cannot open log for append");
    storageError = true;
    return;
  }
  char line[144];
  const int n = snprintf(line, sizeof(line), "%lu,%s\n", static_cast<unsigned long>(millis()), text);
  bool ok = n > 0 && static_cast<size_t>(n) < sizeof(line) && file.fileSize() <= MAX_FILE - n;
  if (ok) ok = file.write(line, n) == static_cast<size_t>(n);
  if (ok) ok = file.sync();
  const bool closed = file.close();
  if (!ok || !closed) {
    LOG_ERR("EventLogger", "Append failed or log full; preserve existing file");
    storageError = true;
    return;
  }
  loadEntries();
}

void EventLoggerActivity::compose() {
  if (storageError) return;
  // Shared keyboard is a cold-path allocation, released by ActivityManager.
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_EVENT_NEW), "", 127);
  if (!keyboard) {
    LOG_ERR("EventLogger", "Cannot allocate keyboard");
    return;
  }
  composing = true;
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    composing = false;
    const auto* value = std::get_if<KeyboardResult>(&result.data);
    if (!result.isCancelled && value && !value->text.empty()) saveEntry(value->text.c_str());
    requestUpdate();
  });
}

void EventLoggerActivity::loop() {
  if (composing || !entries) return;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (viewing) {
      viewing = false;
      requestUpdate();
    } else
      finish();
    return;
  }
  if (viewing) return;
  if (count && mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    selected = (selected + count - 1) % count;
    requestUpdate();
  }
  if (count && mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    selected = (selected + 1) % count;
    requestUpdate();
  }
  if (count && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    viewing = true;
    requestUpdate();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Right))
    compose();
}

void EventLoggerActivity::render(RenderLock&&) {
  renderer.clearScreen();
  if (!entries) {
    renderer.displayBuffer();
    return;
  }
  const auto& m = UITheme::getInstance().getMetrics();
  int mt, mr, mb, ml;
  renderer.getOrientedViewableTRBL(&mt, &mr, &mb, &ml);
  const int width = renderer.getScreenWidth() - ml - mr;
  const int top = mt + m.topPadding;
  GUI.drawHeader(renderer, Rect{ml, top, width, m.headerHeight}, tr(STR_APP_EVENT_LOGGER));
  int y = top + m.headerHeight + 12;
  if (storageError) {
    renderer.drawCenteredText(SMALL_FONT_ID, y, tr(STR_EVENT_STORAGE_ERROR));
    y += 30;
  }
  if (!count)
    renderer.drawCenteredText(UI_10_FONT_ID, y + 30, tr(STR_EVENT_EMPTY));
  else if (viewing) {
    char stamp[64];
    snprintf(stamp, sizeof(stamp), tr(STR_EVENT_UPTIME), static_cast<unsigned long>(entry(selected).uptime / 1000));
    renderer.drawCenteredText(SMALL_FONT_ID, y, stamp);
    y += 30;
    const char* text = entry(selected).text;
    // Bounded UTF-8-aware line slicing; no string/vector allocations during render.
    while (*text && y < renderer.getScreenHeight() - mb - m.buttonHintsHeight - 25) {
      char line[128];
      size_t length = 0;
      while (text[length]) {
        size_t end = length + 1;
        while (text[end] && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) ++end;
        memcpy(line, text, end);
        line[end] = 0;
        if (renderer.getTextWidth(UI_10_FONT_ID, line) > width - 24 && length) break;
        length = end;
      }
      memcpy(line, text, length);
      line[length] = 0;
      renderer.drawText(UI_10_FONT_ID, ml + 12, y, line);
      text += length;
      y += renderer.getLineHeight(UI_10_FONT_ID);
    }
  } else {
    const int rows = std::max(1, (renderer.getScreenHeight() - mb - m.buttonHintsHeight - y) / 48);
    const int first = selected / rows * rows;
    for (int i = first; i < std::min(first + rows, count); ++i) {
      char preview[40];
      size_t n = std::min(strlen(entry(i).text), sizeof(preview) - 1);
      while (n && (static_cast<unsigned char>(entry(i).text[n]) & 0xC0) == 0x80) --n;
      memcpy(preview, entry(i).text, n);
      preview[n] = 0;
      if (i == selected) renderer.drawRect(ml + 6, y, width - 12, 44);
      renderer.drawText(UI_10_FONT_ID, ml + 12, y + 8, preview);
      y += 48;
    }
  }
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), viewing ? "" : tr(STR_EVENT_VIEW), "", viewing ? "" : tr(STR_EVENT_NEW));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
