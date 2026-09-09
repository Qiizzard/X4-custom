// Derived from biscuit, MIT license, Copyright (c) 2025 Dave Allie.
// Adapted for CrossInk; see PORT_NOTES.md.
#include "SnakeActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

// 25% gray (light) — every other pixel in checkerboard on even rows only
static void fillDithered25(GfxRenderer& r, int x, int y, int w, int h) {
  for (int dy = 0; dy < h; dy += 2)
    for (int dx = ((dy / 2) % 2); dx < w; dx += 2) r.drawPixel(x + dx, y + dy, true);
}

void SnakeActivity::onEnter() {
  Activity::onEnter();
  rngState = static_cast<uint32_t>(millis()) | 1u;
  // 6144 bytes retained for one activity; too large for the task stack.
  snake = makeUniqueNoThrow<Point[]>(MAX_CELLS);
  if (!snake) {
    LOG_ERR("Snake", "Cannot allocate snake storage");
    finish();
    return;
  }
  initGame();
}

void SnakeActivity::onExit() {
  snake.reset();
  Activity::onExit();
}

void SnakeActivity::initGame() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  int screenW = renderer.getScreenWidth();
  int screenH = renderer.getScreenHeight();

  // Reserve top area for score and bottom for button hints
  int topReserve = metrics.topPadding + 25;
  int bottomReserve = metrics.buttonHintsHeight;

  int mt, mr, mb, ml;
  renderer.getOrientedViewableTRBL(&mt, &mr, &mb, &ml);
  topReserve += mt;
  bottomReserve += mb + 35;
  gridW = std::clamp((screenW - ml - mr - 8) / CELL_SIZE, 3, 32);
  gridH = std::clamp((screenH - topReserve - bottomReserve) / CELL_SIZE, 3, 48);
  offsetX = ml + (screenW - ml - mr - gridW * CELL_SIZE) / 2;
  offsetY = topReserve;

  snakeLength = 3;
  int startX = std::max(2, gridW / 2);
  int startY = gridH / 2;
  for (int i = 0; i < snakeLength; ++i) snake[i] = {static_cast<int16_t>(startX - i), static_cast<int16_t>(startY)};

  dirX = 1;
  dirY = 0;
  nextDirX = 1;
  nextDirY = 0;
  score = 0;
  state = PLAYING;
  lastStepMs = millis();

  spawnFood();
  requestUpdate();
}

void SnakeActivity::spawnFood() {
  const int cells = gridW * gridH;
  if (snakeLength == cells) {
    state = GAME_OVER;
    return;
  }
  int index = nextRandom() % cells;
  for (int i = 0; i < cells; ++i) {
    food = {static_cast<int16_t>(index % gridW), static_cast<int16_t>(index / gridW)};
    if (!isSnakeAt(food.x, food.y)) return;
    index = (index + 1) % cells;
  }
}

bool SnakeActivity::isSnakeAt(int x, int y) const {
  for (int i = 0; i < snakeLength; ++i) {
    const auto& seg = snake[i];
    if (seg.x == x && seg.y == y) return true;
  }
  return false;
}

void SnakeActivity::step() {
  // Apply buffered direction
  dirX = nextDirX;
  dirY = nextDirY;

  Point head = snake[0];
  Point newHead = {static_cast<int16_t>(head.x + dirX), static_cast<int16_t>(head.y + dirY)};

  // Wall collision
  if (newHead.x < 0 || newHead.x >= gridW || newHead.y < 0 || newHead.y >= gridH) {
    state = GAME_OVER;
    requestUpdate();
    return;
  }

  const bool growing = newHead.x == food.x && newHead.y == food.y;
  // A moving tail vacates its cell in the same step.
  for (int i = 0; i < snakeLength - (growing ? 0 : 1); ++i) {
    if (snake[i].x == newHead.x && snake[i].y == newHead.y) {
      state = GAME_OVER;
      requestUpdate();
      return;
    }
  }
  if (growing) ++snakeLength;  // food exists only while a free board cell remains
  for (int i = snakeLength - 1; i > 0; --i) snake[i] = snake[i - 1];
  snake[0] = newHead;
  if (growing) {
    score += 10;
    spawnFood();
  }

  requestUpdate();
}

void SnakeActivity::loop() {
  if (state == GAME_OVER) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      initGame();
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      finish();
    }
    return;
  }

  // Direction input - prevent reversal
  if (mappedInput.wasPressed(MappedInputManager::Button::Up) && dirY == 0) {
    nextDirX = 0;
    nextDirY = -1;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down) && dirY == 0) {
    nextDirX = 0;
    nextDirY = 1;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Left) && dirX == 0) {
    nextDirX = -1;
    nextDirY = 0;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Right) && dirX == 0) {
    nextDirX = 1;
    nextDirY = 0;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  // Step timer
  unsigned long now = millis();
  if (now - lastStepMs >= STEP_INTERVAL_MS) {
    lastStepMs = now;
    step();
  }
}

void SnakeActivity::render(RenderLock&&) {
  renderer.clearScreen();

  switch (state) {
    case PLAYING:
      renderPlaying();
      break;
    case GAME_OVER:
      renderGameOver();
      break;
  }

  renderer.displayBuffer();
}

void SnakeActivity::renderPlaying() const {
  const auto& metrics = UITheme::getInstance().getMetrics();

  // Double-line border
  renderer.drawRect(offsetX - 1, offsetY - 1, gridW * CELL_SIZE + 2, gridH * CELL_SIZE + 2);
  renderer.drawRect(offsetX - 3, offsetY - 3, gridW * CELL_SIZE + 6, gridH * CELL_SIZE + 6);

  // Subtle background texture
  fillDithered25(renderer, offsetX, offsetY, gridW * CELL_SIZE, gridH * CELL_SIZE);

  // Snake body (with 1px white gap between segments for articulated look)
  for (int i = 0; i < snakeLength; i++) {
    int px = offsetX + snake[i].x * CELL_SIZE;
    int py = offsetY + snake[i].y * CELL_SIZE;
    if (i == 0) {
      // Head — filled with eyes
      renderer.fillRect(px + 1, py + 1, CELL_SIZE - 2, CELL_SIZE - 2);
      // Eyes based on direction
      if (dirX == 1) {  // right
        renderer.drawPixel(px + CELL_SIZE - 3, py + 2, false);
        renderer.drawPixel(px + CELL_SIZE - 3, py + CELL_SIZE - 3, false);
      } else if (dirX == -1) {  // left
        renderer.drawPixel(px + 2, py + 2, false);
        renderer.drawPixel(px + 2, py + CELL_SIZE - 3, false);
      } else if (dirY == -1) {  // up
        renderer.drawPixel(px + 2, py + 2, false);
        renderer.drawPixel(px + CELL_SIZE - 3, py + 2, false);
      } else {  // down
        renderer.drawPixel(px + 2, py + CELL_SIZE - 3, false);
        renderer.drawPixel(px + CELL_SIZE - 3, py + CELL_SIZE - 3, false);
      }
    } else {
      // Body segment with 1px gap (draw slightly smaller)
      renderer.fillRect(px + 2, py + 2, CELL_SIZE - 4, CELL_SIZE - 4);
    }
  }

  // Food — apple shape
  {
    int px = offsetX + food.x * CELL_SIZE;
    int py = offsetY + food.y * CELL_SIZE;
    int cx = px + CELL_SIZE / 2;
    int cy = py + CELL_SIZE / 2 + 1;
    int r = CELL_SIZE / 3;
    // Filled circle body
    for (int dy = -r; dy <= r; dy++) {
      int dx = 0;
      while ((dx + 1) * (dx + 1) + dy * dy <= r * r) dx++;
      if (dx > 0)
        renderer.fillRect(cx - dx, cy + dy, dx * 2 + 1, 1, true);
      else
        renderer.drawPixel(cx, cy + dy, true);
    }
    // Stem on top
    renderer.fillRect(cx, cy - r - 2, 2, 3, true);
  }

  // Score (drawn below the grid)
  int scoreY = offsetY + gridH * CELL_SIZE + 10;
  char scoreBuf[48];
  snprintf(scoreBuf, sizeof(scoreBuf), tr(STR_SNAKE_SCORE_LENGTH), score, (int)snakeLength);
  renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, scoreY, scoreBuf, true, EpdFontFamily::BOLD);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void SnakeActivity::renderGameOver() const {
  const auto pageHeight = renderer.getScreenHeight();
  int y = pageHeight / 2 - 40;

  renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_GAME_OVER), true, EpdFontFamily::BOLD);
  y += 40;

  char scoreBuf[32];
  snprintf(scoreBuf, sizeof(scoreBuf), tr(STR_GAME_SCORE), score);
  renderer.drawCenteredText(UI_10_FONT_ID, y, scoreBuf);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_RETRY), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
