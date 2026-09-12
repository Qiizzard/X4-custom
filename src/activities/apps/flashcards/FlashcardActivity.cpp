// Adapted from biscuit, MIT, Copyright (c) 2025 Dave Allie.
#include "FlashcardActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void FlashcardActivity::scanDecks() {
  deckCount = 0;
  loadError = false;
  scanLimited = false;
  if (!Storage.exists(DECK_DIR)) return;
  auto dir = Storage.open(DECK_DIR);
  if (!dir || !dir.isDirectory()) {
    LOG_ERR("Flashcards", "Cannot open deck directory");
    loadError = true;
    if (dir) dir.close();
    return;
  }
  int inspected = 0;
  while (inspected < 512 && deckCount < MAX_DECKS) {
    auto file = dir.openNextFile();
    if (!file) break;
    ++inspected;
    char name[65]{};
    file.getName(name, sizeof(name));
    const bool directory = file.isDirectory();
    if (!file.close()) {
      loadError = true;
      break;
    }
    const size_t length = strlen(name);
    if (!directory && length > 4 && length < 64 && !strchr(name, '/') && strcmp(name + length - 4, ".csv") == 0) {
      memcpy(data->names[deckCount++], name, length + 1);
    }
  }
  scanLimited = inspected == 512 || deckCount == MAX_DECKS;
  if (!dir.close()) loadError = true;
  if (loadError) LOG_ERR("Flashcards", "Deck directory read/close failed");
}

bool FlashcardActivity::loadDeck() {
  cardCount = 0;
  loadError = false;
  char path[96];
  snprintf(path, sizeof(path), "%s/%s", DECK_DIR, data->names[deckIndex]);
  auto file = Storage.open(path);
  if (!file) {
    LOG_ERR("Flashcards", "Cannot open deck");
    loadError = true;
    return false;
  }
  // At most 32 cards of 127 bytes per side; bound both memory and read work.
  if (file.fileSize() > 16384) loadError = true;
  size_t length = 0;
  auto parseLine = [this, &length]() {
    if (length && data->line[length - 1] == '\r') --length;
    data->line[length] = 0;
    if (length == 0) return true;
    char* comma = strchr(data->line, ',');
    if (!comma || comma == data->line || !comma[1] || cardCount == MAX_CARDS) return false;
    const size_t frontLen = comma - data->line, backLen = strlen(comma + 1);
    if (frontLen > 127 || backLen > 127) return false;
    Card& card = data->cards[cardCount++];
    memcpy(card.front, data->line, frontLen);
    card.front[frontLen] = 0;
    memcpy(card.back, comma + 1, backLen + 1);
    card.correct = card.wrong = 0;
    return true;
  };
  while (!loadError && file.available()) {
    const int c = file.read();
    if (c < 0 || c == 0) {
      loadError = true;
      break;
    }
    if (c == '\n') {
      loadError = !parseLine();
      length = 0;
    } else if (length == sizeof(data->line) - 1)
      loadError = true;
    else
      data->line[length++] = static_cast<char>(c);
  }
  if (!loadError && length) loadError = !parseLine();
  if (!file.close()) loadError = true;
  if (!cardCount) loadError = true;
  if (loadError) {
    LOG_ERR("Flashcards", "Invalid/oversized deck or read failure");
    cardCount = 0;
  }
  return !loadError;
}

void FlashcardActivity::onEnter() {
  Activity::onEnter();
  data = makeUniqueNoThrow<DeckData>();
  if (!data) {
    LOG_ERR("Flashcards", "Cannot allocate deck buffers");
    finish();
    return;
  }
  scanDecks();
  state = DECK_SELECT;
  deckIndex = 0;
  cardIndex = 0;
  requestUpdate();
}

void FlashcardActivity::onExit() {
  data.reset();
  Activity::onExit();
}

void FlashcardActivity::loop() {
  if (state == DECK_SELECT) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      finish();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
      deckIndex = deckCount ? (deckIndex + deckCount - 1) % deckCount : 0;
      requestUpdate();
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
      deckIndex = deckCount ? (deckIndex + 1) % deckCount : 0;
      requestUpdate();
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm) && deckCount > 0) {
      if (loadDeck()) {
        cardIndex = 0;
        state = CARD_FRONT;
      }
      requestUpdate();
    }
    return;
  }

  if (state == CARD_FRONT) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      state = DECK_SELECT;
      requestUpdate();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      state = CARD_BACK;
      requestUpdate();
    }
    return;
  }

  if (state == CARD_BACK) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      state = DECK_SELECT;
      requestUpdate();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
      data->cards[cardIndex].wrong++;
      cardIndex++;
      if (cardIndex >= cardCount) {
        state = STATS;
        requestUpdate();
        return;
      }
      state = CARD_FRONT;
      requestUpdate();
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      data->cards[cardIndex].correct++;
      cardIndex++;
      if (cardIndex >= cardCount) {
        state = STATS;
        requestUpdate();
        return;
      }
      state = CARD_FRONT;
      requestUpdate();
    }
    return;
  }

  if (state == STATS) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      state = DECK_SELECT;
      requestUpdate();
    }
    return;
  }
}

void FlashcardActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_APP_FLASHCARDS));

  switch (state) {
    case DECK_SELECT:
      renderDeckSelect();
      break;
    case CARD_FRONT:
      renderCardFront();
      break;
    case CARD_BACK:
      renderCardBack();
      break;
    case STATS:
      renderStats();
      break;
  }
  renderer.displayBuffer();
}

void FlashcardActivity::renderDeckSelect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int listTop = metrics.topPadding + metrics.headerHeight;
  const int listH = pageHeight - listTop - metrics.buttonHintsHeight;

  if (loadError)
    renderer.drawCenteredText(SMALL_FONT_ID, listTop + 5, tr(STR_FLASH_ERROR));
  else if (scanLimited)
    renderer.drawCenteredText(SMALL_FONT_ID, listTop + 5, tr(STR_FLASH_LIMIT));
  if (deckCount == 0) {
    renderer.drawCenteredText(UI_10_FONT_ID, listTop + listH / 2, tr(STR_FLASH_EMPTY));
  } else {
    const int rows = std::max(1, (listH - 60) / 48);
    const int first = deckIndex / rows * rows;
    for (int i = first; i < std::min(first + rows, deckCount); ++i) {
      const int y = listTop + 55 + (i - first) * 48;
      if (i == deckIndex) renderer.drawRect(12, y, pageWidth - 24, 44);
      drawCardText(data->names[i], y + 4, y + 42);
    }
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_FLASH_LOAD), "^", "v");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void FlashcardActivity::renderCardFront() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int mid = (metrics.topPadding + metrics.headerHeight + pageHeight - metrics.buttonHintsHeight) / 2;

  char prog[24];
  snprintf(prog, sizeof(prog), tr(STR_FLASH_PROGRESS), cardIndex + 1, cardCount);
  renderer.drawCenteredText(SMALL_FONT_ID, mid - 60, prog);
  drawCardText(data->cards[cardIndex].front, mid - 20, pageHeight - metrics.buttonHintsHeight - 10);

  const auto labels = mappedInput.mapLabels(tr(STR_FLASH_QUIT), tr(STR_FLASH_FLIP), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void FlashcardActivity::renderCardBack() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int mid = (metrics.topPadding + metrics.headerHeight + pageHeight - metrics.buttonHintsHeight) / 2;

  drawCardText(data->cards[cardIndex].back, mid - 60, pageHeight - metrics.buttonHintsHeight - 10);

  const auto labels = mappedInput.mapLabels(tr(STR_FLASH_QUIT), "", tr(STR_FLASH_WRONG), tr(STR_FLASH_CORRECT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void FlashcardActivity::renderStats() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int top = metrics.topPadding + metrics.headerHeight + 30;

  int total = 0, correct = 0, wrong = 0;
  for (int i = 0; i < cardCount; ++i) {
    total++;
    correct += data->cards[i].correct;
    wrong += data->cards[i].wrong;
  }
  int pct = (total > 0) ? (correct * 100 / total) : 0;

  renderer.drawCenteredText(UI_12_FONT_ID, top, tr(STR_FLASH_RESULTS), true, EpdFontFamily::BOLD);

  char buf[48];
  snprintf(buf, sizeof(buf), tr(STR_FLASH_TOTAL), total);
  renderer.drawCenteredText(UI_10_FONT_ID, top + 50, buf);
  snprintf(buf, sizeof(buf), tr(STR_FLASH_CORRECT_COUNT), correct);
  renderer.drawCenteredText(UI_10_FONT_ID, top + 80, buf);
  snprintf(buf, sizeof(buf), tr(STR_FLASH_WRONG_COUNT), wrong);
  renderer.drawCenteredText(UI_10_FONT_ID, top + 110, buf);
  snprintf(buf, sizeof(buf), tr(STR_FLASH_SCORE), pct);
  renderer.drawCenteredText(UI_12_FONT_ID, top + 150, buf, true, EpdFontFamily::BOLD);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void FlashcardActivity::drawCardText(const char* text, int y, int bottom) const {
  int mt, mr, mb, ml;
  renderer.getOrientedViewableTRBL(&mt, &mr, &mb, &ml);
  const int width = renderer.getScreenWidth() - ml - mr - 24;
  bottom = std::min(bottom, renderer.getScreenHeight() - mb);
  while (*text && y + renderer.getLineHeight(UI_10_FONT_ID) <= bottom) {
    char line[128];
    size_t length = 0;
    while (text[length]) {
      size_t end = length + 1;
      while (text[end] && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) ++end;
      memcpy(line, text, end);
      line[end] = 0;
      if (renderer.getTextWidth(UI_10_FONT_ID, line) > width && length) break;
      length = end;
    }
    memcpy(line, text, length);
    line[length] = 0;
    renderer.drawText(UI_10_FONT_ID, ml + 12, y, line);
    text += length;
    y += renderer.getLineHeight(UI_10_FONT_ID);
  }
}
