// Adapted from biscuit, MIT, Copyright (c) 2025 Dave Allie.
#include "HabitTrackerActivity.h"

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

const char* HabitTrackerActivity::slotPath(int slot) {
  return slot == 0 ? "/crossink/habits-a.dat" : "/crossink/habits-b.dat";
}
uint32_t HabitTrackerActivity::checksum(const uint8_t* data, int size) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < size; ++i) hash = (hash ^ data[i]) * 16777619u;
  return hash;
}
uint32_t HabitTrackerActivity::get32(int offset) const {
  uint32_t value = 0;
  for (int i = 0; i < 4; ++i) value |= static_cast<uint32_t>(bytes[offset + i]) << (8 * i);
  return value;
}
void HabitTrackerActivity::put32(int offset, uint32_t value) {
  for (int i = 0; i < 4; ++i) bytes[offset + i] = static_cast<uint8_t>(value >> (8 * i));
}
bool HabitTrackerActivity::readSlot(int slot) {
  auto file = Storage.open(slotPath(slot));
  if (!file) {
    LOG_ERR("Habits", "Cannot open slot %d", slot);
    return false;
  }
  const bool ok = file.fileSize() == sizeof(bytes) && file.read(bytes, sizeof(bytes)) == sizeof(bytes);
  const bool closed = file.close();
  if (!ok || !closed || memcmp(bytes, "HBT1", 4) || bytes[12] > 10 || !get32(8) || get32(423) != checksum(bytes, 423)) {
    LOG_ERR("Habits", "Invalid record in slot %d", slot);
    return false;
  }
  for (int i = 0; i < bytes[12]; ++i) {
    const int offset = 13 + i * 41;
    if (!bytes[offset] || !memchr(bytes + offset, 0, 32) || bytes[offset + 32] > 1 ||
        get32(offset + 37) < get32(offset + 33)) {
      LOG_ERR("Habits", "Invalid habit fields in slot %d", slot);
      return false;
    }
  }
  return true;
}
void HabitTrackerActivity::decode() {
  generation = get32(4);
  session = get32(8);
  count = bytes[12];
  for (int i = 0; i < count; ++i) {
    const int offset = 13 + i * 41;
    memcpy(habits[i].name, bytes + offset, 32);
    habits[i].done = bytes[offset + 32];
    habits[i].streak = get32(offset + 33);
    habits[i].best = get32(offset + 37);
  }
}
void HabitTrackerActivity::load() {
  bool any = false;
  for (int slot = 0; slot < 2; ++slot) {
    if (!Storage.exists(slotPath(slot))) continue;
    any = true;
    if (readSlot(slot) && (activeSlot < 0 || get32(4) > generation)) {
      decode();
      activeSlot = slot;
    }
  }
  blocked = any && activeSlot < 0;
  error = blocked;
  if (blocked) LOG_ERR("Habits", "No valid save; preserve files and block edits");
}
void HabitTrackerActivity::changed() {
  dirty = true;
  changedAt = millis();
  error = false;
  requestUpdate();
}
bool HabitTrackerActivity::save() {
  if (!dirty) return !blocked;
  if (blocked || generation == std::numeric_limits<uint32_t>::max()) {
    LOG_ERR("Habits", "Save blocked or generation exhausted");
    error = true;
    return false;
  }
  if (!Storage.exists("/crossink") && !Storage.mkdir("/crossink", true)) {
    LOG_ERR("Habits", "Cannot create storage directory");
    error = true;
    return false;
  }
  memset(bytes, 0, sizeof(bytes));
  memcpy(bytes, "HBT1", 4);
  put32(4, generation + 1);
  put32(8, session);
  bytes[12] = count;
  for (int i = 0; i < count; ++i) {
    const int offset = 13 + i * 41;
    memcpy(bytes + offset, habits[i].name, 32);
    bytes[offset + 32] = habits[i].done;
    put32(offset + 33, habits[i].streak);
    put32(offset + 37, habits[i].best);
  }
  put32(423, checksum(bytes, 423));
  const int target = activeSlot == 0 ? 1 : 0;
  auto file = Storage.open(slotPath(target), O_WRITE | O_CREAT | O_TRUNC);
  if (!file) {
    LOG_ERR("Habits", "Cannot open save slot");
    error = true;
    return false;
  }
  bool ok = file.write(bytes, sizeof(bytes)) == sizeof(bytes);
  if (ok) ok = file.sync();
  const bool closed = file.close();
  if (!ok || !closed) {
    LOG_ERR("Habits", "Save failed; previous slot retained");
    error = true;
    return false;
  }
  activeSlot = target;
  ++generation;
  dirty = false;
  error = false;
  return true;
}
void HabitTrackerActivity::onEnter() {
  Activity::onEnter();
  load();
  requestUpdate();
}
void HabitTrackerActivity::onExit() {
  if (dirty) save();
  Activity::onExit();
}
void HabitTrackerActivity::addHabit() {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_HABIT_NAME), "", 31);
  if (!keyboard) {
    LOG_ERR("Habits", "Cannot allocate shared keyboard");
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    const auto* value = std::get_if<KeyboardResult>(&result.data);
    if (!result.isCancelled && value && !value->text.empty() && value->text.size() <= 31 && count < 10) {
      habits[count] = Habit{};
      memcpy(habits[count].name, value->text.c_str(), value->text.size() + 1);
      ++count;
      changed();
    }
    requestUpdate();
  });
}
void HabitTrackerActivity::loop() {
  if (dirty && !error && millis() - changedAt >= 750) {
    save();
    requestUpdate();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (screen != Screen::Main) {
      screen = Screen::Main;
      selected = 0;
      requestUpdate();
    } else if (blocked || save())
      finish();
    else
      requestUpdate();
    return;
  }
  if (blocked) return;
  if (error && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    save();
    requestUpdate();
    return;
  }
  if (screen == Screen::NewSession || screen == Screen::Delete) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (screen == Screen::Delete) {
        for (int i = selected; i + 1 < count; ++i) habits[i] = habits[i + 1];
        --count;
        selected = 0;
      } else if (session < std::numeric_limits<uint32_t>::max()) {
        ++session;
        for (int i = 0; i < count; ++i) {
          auto& h = habits[i];
          if (h.done) {
            if (h.streak < std::numeric_limits<uint32_t>::max()) ++h.streak;
            h.best = std::max(h.best, h.streak);
          } else
            h.streak = 0;
          h.done = false;
        }
      } else {
        LOG_ERR("Habits", "Session counter exhausted");
        return;
      }
      screen = Screen::Main;
      changed();
    }
    return;
  }
  const int total = count + (screen == Screen::Edit && count < 10 ? 1 : 0);
  if (total && mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    selected = (selected + total - 1) % total;
    requestUpdate();
  }
  if (total && mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    selected = (selected + 1) % total;
    requestUpdate();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    screen = screen == Screen::Main ? Screen::Edit : Screen::Main;
    selected = 0;
    requestUpdate();
    return;
  }
  if (screen == Screen::Main && mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
    screen = Screen::NewSession;
    requestUpdate();
    return;
  }
  if (total && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (screen == Screen::Main) {
      habits[selected].done = !habits[selected].done;
      changed();
    } else if (selected == count)
      addHabit();
    else {
      screen = Screen::Delete;
      requestUpdate();
    }
  }
}
void HabitTrackerActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& m = UITheme::getInstance().getMetrics();
  int mt, mr, mb, ml;
  renderer.getOrientedViewableTRBL(&mt, &mr, &mb, &ml);
  const int width = renderer.getScreenWidth() - ml - mr, top = mt + m.topPadding;
  GUI.drawHeader(renderer, Rect{ml, top, width, m.headerHeight}, tr(STR_APP_HABITS));
  int y = top + m.headerHeight + 8;
  char status[80];
  snprintf(status, sizeof(status), tr(STR_HABIT_SESSION), static_cast<unsigned long>(session));
  renderer.drawCenteredText(SMALL_FONT_ID, y,
                            blocked ? tr(STR_HABIT_CORRUPT)
                            : error ? tr(STR_HABIT_ERROR)
                            : dirty ? tr(STR_HABIT_UNSAVED)
                                    : status);
  y += 30;
  if (screen == Screen::NewSession || screen == Screen::Delete) {
    renderer.drawCenteredText(UI_10_FONT_ID, y + 30,
                              screen == Screen::Delete ? tr(STR_HABIT_DELETE) : tr(STR_HABIT_ADVANCE));
  } else {
    const int total = count + (screen == Screen::Edit && count < 10 ? 1 : 0);
    const int rows = std::max(1, (renderer.getScreenHeight() - mb - m.buttonHintsHeight - y - 30) / 65);
    const int first = selected / rows * rows;
    if (!total) renderer.drawCenteredText(UI_10_FONT_ID, y + 30, tr(STR_HABIT_EMPTY));
    for (int i = first; i < std::min(first + rows, total); ++i) {
      if (i == selected) renderer.drawRect(ml + 8, y, width - 16, 60);
      if (i == count)
        renderer.drawText(UI_10_FONT_ID, ml + 14, y + 5, tr(STR_HABIT_ADD));
      else {
        char label[40];
        snprintf(label, sizeof(label), "%s %s", habits[i].done ? "[x]" : "[ ]", habits[i].name);
        renderer.drawText(UI_10_FONT_ID, ml + 14, y + 4, label);
        char streak[80];
        snprintf(streak, sizeof(streak), tr(STR_HABIT_STREAK), static_cast<unsigned long>(habits[i].streak),
                 static_cast<unsigned long>(habits[i].best));
        renderer.drawText(SMALL_FONT_ID, ml + 14, y + 32, streak);
      }
      y += 65;
    }
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), "", tr(STR_HABIT_EDIT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
