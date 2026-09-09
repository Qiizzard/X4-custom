#include "GameOfLifeActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

uint32_t GameOfLifeActivity::nextRandom() {
  rngState ^= rngState << 13;
  rngState ^= rngState >> 17;
  rngState ^= rngState << 5;
  return rngState;
}

bool GameOfLifeActivity::cellAt(const uint8_t* board, const int x, const int y) const {
  // Toroidal wraparound: the board has no edges, so a glider (etc.) never
  // just runs off and dies against a hard boundary.
  const int wx = ((x % kGridWidth) + kGridWidth) % kGridWidth;
  const int wy = ((y % kGridHeight) + kGridHeight) % kGridHeight;
  return (board[wy * kRowBytes + wx / 8] & (1u << (wx % 8))) != 0;
}

void GameOfLifeActivity::setCellAt(uint8_t* board, const int x, const int y, const bool alive) const {
  uint8_t& byte = board[y * kRowBytes + x / 8];
  const uint8_t mask = static_cast<uint8_t>(1u << (x % 8));
  if (alive) {
    byte |= mask;
  } else {
    byte &= ~mask;
  }
}

void GameOfLifeActivity::seedRandom() {
  if (!boardReady) return;
  std::memset(current.get(), 0, kBufferBytes);
  // ~35% live density: dense enough for interesting early activity, sparse
  // enough to not just be a solid block that immediately dies of overcrowding.
  for (int y = 0; y < kGridHeight; ++y) {
    for (int x = 0; x < kGridWidth; ++x) {
      if ((nextRandom() % 100) < 35) setCellAt(current.get(), x, y, true);
    }
  }
  generation = 0;
  liveCount = 0;
  for (size_t i = 0; i < kBufferBytes; ++i) {
    liveCount += static_cast<uint32_t>(__builtin_popcount(current[i]));
  }
}

void GameOfLifeActivity::step() {
  if (!boardReady) return;
  std::memset(next.get(), 0, kBufferBytes);
  uint32_t count = 0;
  for (int y = 0; y < kGridHeight; ++y) {
    for (int x = 0; x < kGridWidth; ++x) {
      int neighbors = 0;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0) continue;
          if (cellAt(current.get(), x + dx, y + dy)) ++neighbors;
        }
      }
      const bool alive = cellAt(current.get(), x, y);
      // Standard Conway rules: a live cell survives on 2 or 3 neighbors, a
      // dead cell is born on exactly 3.
      const bool nextAlive = alive ? (neighbors == 2 || neighbors == 3) : (neighbors == 3);
      if (nextAlive) {
        setCellAt(next.get(), x, y, true);
        ++count;
      }
    }
  }
  std::swap(current, next);
  liveCount = count;
  ++generation;
}

void GameOfLifeActivity::onEnter() {
  Activity::onEnter();
  current = makeUniqueNoThrow<uint8_t[]>(kBufferBytes);
  next = makeUniqueNoThrow<uint8_t[]>(kBufferBytes);
  boardReady = current != nullptr && next != nullptr;
  if (!boardReady) {
    // Either allocation can fail on a fragmented C3 heap. Release any
    // successful allocation and leave the error screen safe to exit.
    LOG_ERR("GOL", "Cannot allocate two %u-byte boards", static_cast<unsigned>(kBufferBytes));
    next.reset();
    current.reset();
  }
  rngState = static_cast<uint32_t>(millis()) * 2654435761u;
  if (rngState == 0) rngState = 0x9E3779B9u;
  seedRandom();
  requestUpdate();
}

void GameOfLifeActivity::onExit() {
  next.reset();
  current.reset();
  boardReady = false;
  Activity::onExit();
}

void GameOfLifeActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (!boardReady) return;

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    step();
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    seedRandom();
    requestUpdate();
  }
}

void GameOfLifeActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_GAME_OF_LIFE), false);
  } else {
    GUI.drawHeader(renderer, header, tr(STR_APP_GAME_OF_LIFE));
  }

  const int contentTop = header.y + header.height + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int statusLineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int boardTop = contentTop + statusLineHeight + metrics.verticalSpacing;
  const int boardHeight = contentBottom - boardTop;

  char status[64];
  if (!boardReady) {
    renderer.drawCenteredText(UI_10_FONT_ID, contentTop, tr(STR_GOL_OOM), true);
  } else {
    snprintf(status, sizeof(status), "%s %lu   %s %lu", tr(STR_GOL_GENERATION_LBL),
             static_cast<unsigned long>(generation), tr(STR_GOL_ALIVE_LBL), static_cast<unsigned long>(liveCount));
    renderer.drawCenteredText(UI_10_FONT_ID, contentTop, status, true);

    // Scale to fit whichever dimension is tighter, floored to at least 1px --
    // the board itself has a fixed cell count, only the on-screen cell size
    // varies by device.
    const int cellSize = std::max(1, std::min(pageWidth / kGridWidth, boardHeight / kGridHeight));
    const int boardPxWidth = cellSize * kGridWidth;
    const int boardPxHeight = cellSize * kGridHeight;
    const int originX = (pageWidth - boardPxWidth) / 2;
    const int originY = boardTop + std::max(0, (boardHeight - boardPxHeight) / 2);

    for (int y = 0; y < kGridHeight; ++y) {
      for (int x = 0; x < kGridWidth; ++x) {
        if (cellAt(current.get(), x, y)) {
          renderer.fillRect(originX + x * cellSize, originY + y * cellSize, cellSize, cellSize, true);
        }
      }
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_GOL_STEP), tr(STR_GOL_RESTART), "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
