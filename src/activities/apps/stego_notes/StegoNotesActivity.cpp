#include "StegoNotesActivity.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "activities/apps/secure_vault/SecretEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr char kDrawings[] = "/crossink/drawings";
constexpr char kStego[] = "/crossink/stego";
bool validName(const char* name) {
  const size_t n = strlen(name);
  if (n < 5 || n >= 64 || name[0] == '.') return false;
  for (size_t i = 0; i < n; ++i) {
    const char c = name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
          c == '.'))
      return false;
  }
  return strcmp(name + n - 4, ".bmp") == 0 || strcmp(name + n - 4, ".BMP") == 0;
}
}  // namespace
StegoNotesActivity::~StegoNotesActivity() { wipe(); }
void StegoNotesActivity::wipe() {
  securestore::secureZero(note, sizeof(note));
  securestore::secureZero(entry, sizeof(entry));
  securestore::secureZero(key, sizeof(key));
  securestore::secureZero(&workspace, sizeof(workspace));
  length = page = 0;
}
void StegoNotesActivity::reset() {
  wipe();
  screen = Screen::Mode;
  selected = 0;
  lastInput = millis();
  requestUpdate();
}
void StegoNotesActivity::fail() {
  LOG_ERR("StegoNotes", "Operation failed; clearing secret state");
  wipe();
  screen = Screen::Error;
  requestUpdate();
}
void StegoNotesActivity::onEnter() {
  Activity::onEnter();
  reset();
}
void StegoNotesActivity::onExit() {
  wipe();
  renderer.clearScreen();
  Activity::onExit();
}
void StegoNotesActivity::scan() {
  count = selected = 0;
  limited = false;
  const char* directory = extract ? kStego : kDrawings;
  screen = Screen::Files;
  if (!Storage.exists(directory)) return;
  auto dir = Storage.open(directory);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    fail();
    return;
  }
  int inspected = 0;
  bool ok = true;
  while (count < 16 && inspected < 512) {
    auto file = dir.openNextFile();
    if (!file) break;
    ++inspected;
    char name[65]{};
    file.getName(name, sizeof(name));
    const bool directoryEntry = file.isDirectory();
    if (!file.close()) {
      ok = false;
      break;
    }
    if (!directoryEntry && validName(name)) memcpy(names[count++], name, strlen(name) + 1);
  }
  limited = count == 16 || inspected == 512;
  if (!dir.close()) ok = false;
  if (!ok) fail();
}
bool StegoNotesActivity::outputPath() {
  if (!Storage.exists(kStego) && !Storage.mkdir(kStego)) return false;
  for (int i = 0; i < 100; ++i) {
    snprintf(destination, sizeof(destination), "%s/note-%02d.bmp", kStego, i);
    if (!Storage.exists(destination)) return true;
  }
  return false;
}
void StegoNotesActivity::ask(Input step) {
  inputStep = step;
  size_t capacity = sizeof(entry), minimum = 8;
  StrId prompt = step == Input::ConfirmKey ? StrId::STR_VAULT_CONFIRM_KEY : StrId::STR_VAULT_KEY;
  if (step == Input::Text) {
    const size_t remaining = stegobmp::kMaxNote - length;
    if (remaining <= (length ? 1u : 0u)) return;
    capacity = std::min(sizeof(entry), remaining - (length ? 1 : 0) + 1);
    minimum = 1;
    prompt = StrId::STR_STEGO_TEXT;
  }
  // Small fixed-buffer child; no plaintext std::string result copies.
  auto child = makeUniqueNoThrow<SecretEntryActivity>(renderer, mappedInput, prompt, entry, capacity, minimum);
  if (!child) {
    LOG_ERR("StegoNotes", "Input allocation failed (%u bytes)", unsigned(sizeof(SecretEntryActivity)));
    fail();
    return;
  }
  startActivityForResult(std::move(child), [this](const ActivityResult& result) {
    RenderLock lock;
    accept(result.isCancelled);
  });
}
void StegoNotesActivity::accept(bool cancelled) {
  lastInput = millis();
  if (cancelled) {
    reset();
    return;
  }
  if (inputStep == Input::Text) {
    const size_t added = strlen(entry), separator = length ? 1 : 0;
    if (!added || added + separator > stegobmp::kMaxNote - length) {
      fail();
      return;
    }
    if (separator) note[length++] = '\n';
    memcpy(note + length, entry, added);
    length += added;
    note[length] = 0;
    securestore::secureZero(entry, sizeof(entry));
    screen = Screen::Compose;
    return;
  }
  if (inputStep == Input::Key) {
    memcpy(key, entry, sizeof(key));
    securestore::secureZero(entry, sizeof(entry));
    if (!extract) {
      ask(Input::ConfirmKey);
      return;
    }
    const bool ok =
        stegobmp::reveal(source, key, reinterpret_cast<uint8_t*>(note), stegobmp::kMaxNote, length, workspace);
    securestore::secureZero(key, sizeof(key));
    if (!ok) {
      fail();
      return;
    }
    // Render only the supported printable text format, never control sequences.
    for (size_t i = 0; i < length; ++i) {
      const unsigned char c = note[i];
      if (c != '\n' && (c < 32 || c > 126)) {
        fail();
        return;
      }
    }
    note[length] = 0;
    screen = Screen::View;
    page = 0;
    revealedAt = lastInput = millis();
    return;
  }
  unsigned difference = 0;
  for (size_t i = 0; i < sizeof(entry); ++i) difference |= entry[i] ^ key[i];
  securestore::secureZero(entry, sizeof(entry));
  if (difference || !outputPath() ||
      !stegobmp::hide(source, destination, key, reinterpret_cast<const uint8_t*>(note), length, workspace)) {
    fail();
    return;
  }
  wipe();
  screen = Screen::Saved;
  lastInput = millis();
}
void StegoNotesActivity::loop() {
  RenderLock lock;
  if ((screen != Screen::Mode && millis() - lastInput >= 60000) ||
      (screen == Screen::View && millis() - revealedAt >= 10000)) {
    reset();
    return;
  }
  using Button = MappedInputManager::Button;
  const bool back = mappedInput.wasReleased(Button::Back), confirm = mappedInput.wasReleased(Button::Confirm);
  const bool up = mappedInput.wasReleased(Button::Up), down = mappedInput.wasReleased(Button::Down);
  if (!(back || confirm || up || down)) return;
  lastInput = millis();
  if (back) {
    if (screen == Screen::Mode)
      finish();
    else
      reset();
    return;
  }
  if (screen == Screen::Mode || screen == Screen::Compose) {
    if (up || down) selected = 1 - selected;
    if (confirm) {
      if (screen == Screen::Mode) {
        extract = selected == 1;
        scan();
      } else if (selected == 0)
        ask(Input::Text);
      else if (length)
        ask(Input::Key);
    }
  } else if (screen == Screen::Files && count) {
    if (up) selected = (selected + count - 1) % count;
    if (down) selected = (selected + 1) % count;
    if (confirm) {
      snprintf(source, sizeof(source), "%s/%s", extract ? kStego : kDrawings, names[selected]);
      selected = 0;
      if (extract)
        ask(Input::Key);
      else
        screen = Screen::Compose;
    }
  } else if (screen == Screen::View) {
    if (up && page) --page;
    if (down && (page + 1) * 64 < length) ++page;
  } else if ((screen == Screen::Saved || screen == Screen::Error) && confirm)
    reset();
  requestUpdate();
}
void StegoNotesActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& m = UITheme::getInstance().getMetrics();
  int mt, mr, mb, ml;
  renderer.getOrientedViewableTRBL(&mt, &mr, &mb, &ml);
  const int width = renderer.getScreenWidth() - ml - mr;
  GUI.drawHeader(renderer, Rect{ml, mt + m.topPadding, width, m.headerHeight}, tr(STR_STEGO_APP));
  int y = mt + m.topPadding + m.headerHeight + 15;
  if (screen == Screen::Mode || screen == Screen::Compose) {
    renderer.drawCenteredText(SMALL_FONT_ID, y, tr(STR_STEGO_NOTICE));
    y += 40;
    for (int i = 0; i < 2; ++i) {
      const char* label = screen == Screen::Mode ? (i ? tr(STR_STEGO_REVEAL) : tr(STR_STEGO_EMBED))
                                                 : (i ? tr(STR_STEGO_WRITE) : tr(STR_STEGO_APPEND));
      renderer.drawText(UI_10_FONT_ID, ml + 10, y, selected == i ? ">" : "");
      renderer.drawText(UI_10_FONT_ID, ml + 35, y, label);
      y += 40;
    }
    if (screen == Screen::Compose) {
      char status[64];
      snprintf(status, sizeof(status), tr(STR_STEGO_SIZE), unsigned(length));
      renderer.drawCenteredText(SMALL_FONT_ID, y + 10, status);
    }
  } else if (screen == Screen::Files) {
    renderer.drawCenteredText(SMALL_FONT_ID, y, limited ? tr(STR_STEGO_LIMIT) : extract ? kStego : kDrawings);
    y += 30;
    const int rows = std::max(1, (renderer.getScreenHeight() - mb - m.buttonHintsHeight - y) / 40);
    const int first = selected / rows * rows;
    if (!count) renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_STEGO_EMPTY));
    for (int i = first; i < std::min(count, first + rows); ++i) {
      renderer.drawText(UI_10_FONT_ID, ml + 10, y, i == selected ? ">" : "");
      renderer.drawText(UI_10_FONT_ID, ml + 35, y, names[i]);
      y += 40;
    }
  } else if (screen == Screen::View) {
    // 64-byte pages, four fixed 16-character lines; newlines display as spaces.
    // This bounds plaintext stack storage and works on both portrait/landscape.
    char line[17]{};
    size_t pos = page * 64, end = std::min(length, pos + 64);
    while (pos < end && y + 30 < renderer.getScreenHeight() - mb - m.buttonHintsHeight) {
      size_t n = 0;
      while (pos < end && n < 16) {
        const char c = note[pos++];
        line[n++] = c == '\n' ? ' ' : c;
      }
      line[n] = 0;
      renderer.drawText(UI_10_FONT_ID, ml + 15, y, line);
      securestore::secureZero(line, sizeof(line));
      y += 30;
    }
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getScreenHeight() - mb - m.buttonHintsHeight - 25,
                              tr(STR_STEGO_VIEW_HINT));
  } else {
    renderer.drawCenteredText(UI_10_FONT_ID, y, screen == Screen::Saved ? tr(STR_STEGO_SAVED) : tr(STR_STEGO_ERROR));
    if (screen == Screen::Saved) renderer.drawCenteredText(SMALL_FONT_ID, y + 45, destination);
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
