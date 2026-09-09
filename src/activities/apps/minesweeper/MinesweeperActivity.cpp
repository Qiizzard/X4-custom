// Derived from biscuit, MIT license, Copyright (c) 2025 Dave Allie.
// Adapted for CrossInk; see PORT_NOTES.md.
#include "MinesweeperActivity.h"

#include <Arduino.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

// 50% gray — classic checkerboard dither
static void fillDithered50(GfxRenderer& r, int x, int y, int w, int h) {
  for (int dy = 0; dy < h; dy++)
    for (int dx = (dy % 2); dx < w; dx += 2) r.drawPixel(x + dx, y + dy, true);
}

void MinesweeperActivity::onEnter() {
  Activity::onEnter();
  rngState = static_cast<uint32_t>(millis()) | 1u;
  state = DIFFICULTY_SELECT;
  difficultyIndex = 1;
  requestUpdate();
}

void MinesweeperActivity::initGame() {
  memset(grid, 0, sizeof(grid));
  cursorX = cols / 2;
  cursorY = rows / 2;
  flagCount = 0;
  firstReveal = true;
  state = PLAYING;
}

void MinesweeperActivity::placeMines(int safeX, int safeY) {
  int placed = 0;
  while (placed < mineCount) {
    int x = nextRandom() % cols;
    int y = nextRandom() % rows;
    // Don't place on safe cell or adjacent to it
    if (abs(x - safeX) <= 1 && abs(y - safeY) <= 1) continue;
    if (isMine(x, y)) continue;
    grid[x][y] = (grid[x][y] & 0xF0) | MINE;
    placed++;
  }
  calculateNumbers();
}

void MinesweeperActivity::calculateNumbers() {
  for (int x = 0; x < cols; x++) {
    for (int y = 0; y < rows; y++) {
      if (isMine(x, y)) continue;
      grid[x][y] = (grid[x][y] & 0xF0) | static_cast<uint8_t>(countAdjacentMines(x, y));
    }
  }
}

int MinesweeperActivity::countAdjacentMines(int x, int y) const {
  int count = 0;
  for (int dx = -1; dx <= 1; dx++) {
    for (int dy = -1; dy <= 1; dy++) {
      int nx = x + dx, ny = y + dy;
      if (nx >= 0 && nx < cols && ny >= 0 && ny < rows && isMine(nx, ny)) {
        count++;
      }
    }
  }
  return count;
}

void MinesweeperActivity::reveal(int x, int y) {
  if (x < 0 || x >= cols || y < 0 || y >= rows) return;
  if (isRevealed(x, y) || isFlagged(x, y)) return;

  uint8_t queue[MAX_COLS * MAX_ROWS];
  int head = 0, tail = 0;
  grid[x][y] |= REVEALED;
  queue[tail++] = static_cast<uint8_t>(x * MAX_ROWS + y);
  while (head < tail) {
    const int cell = queue[head++];
    const int cx = cell / MAX_ROWS, cy = cell % MAX_ROWS;
    if (getCellValue(cx, cy) != 0) continue;
    for (int dx = -1; dx <= 1; ++dx) {
      for (int dy = -1; dy <= 1; ++dy) {
        const int nx = cx + dx, ny = cy + dy;
        if (nx < 0 || nx >= cols || ny < 0 || ny >= rows || isRevealed(nx, ny) || isFlagged(nx, ny) || isMine(nx, ny))
          continue;
        grid[nx][ny] |= REVEALED;  // mark on enqueue: each cell enters once
        queue[tail++] = static_cast<uint8_t>(nx * MAX_ROWS + ny);
      }
    }
  }
}

void MinesweeperActivity::revealAll() {
  for (int x = 0; x < cols; x++) {
    for (int y = 0; y < rows; y++) {
      grid[x][y] |= REVEALED;
    }
  }
}

bool MinesweeperActivity::checkWin() const {
  for (int x = 0; x < cols; x++) {
    for (int y = 0; y < rows; y++) {
      if (!isMine(x, y) && !isRevealed(x, y)) return false;
    }
  }
  return true;
}

void MinesweeperActivity::loop() {
  if (state == DIFFICULTY_SELECT) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Down) ||
        mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      difficultyIndex = (difficultyIndex + 1) % 3;
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
        mappedInput.wasReleased(MappedInputManager::Button::Left)) {
      difficultyIndex = (difficultyIndex + 2) % 3;
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (difficultyIndex == 0) {
        cols = 8;
        rows = 12;
        mineCount = 10;
      } else if (difficultyIndex == 1) {
        cols = 10;
        rows = 16;
        mineCount = 25;
      } else {
        cols = 10;
        rows = 16;
        mineCount = 40;
      }
      initGame();
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
    }
    return;
  }

  if (state == WON || state == GAME_OVER) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      state = DIFFICULTY_SELECT;
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
    }
    return;
  }

  // PLAYING state
  bool moved = false;
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    if (cursorY > 0) cursorY--;
    moved = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    if (cursorY < rows - 1) cursorY++;
    moved = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    if (cursorX > 0) cursorX--;
    moved = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    if (cursorX < cols - 1) cursorX++;
    moved = true;
  }

  // Confirm: short press = reveal, long press (500ms+) = toggle flag
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    const bool longPress = mappedInput.getHeldTime() >= 500;
    if (!isRevealed(cursorX, cursorY)) {
      if (longPress) {
        // Toggle flag
        if (isFlagged(cursorX, cursorY)) {
          grid[cursorX][cursorY] &= ~FLAGGED;
          flagCount--;
        } else {
          grid[cursorX][cursorY] |= FLAGGED;
          flagCount++;
        }
      } else if (!isFlagged(cursorX, cursorY)) {
        // Reveal
        if (firstReveal) {
          firstReveal = false;
          placeMines(cursorX, cursorY);
        }
        if (isMine(cursorX, cursorY)) {
          revealAll();
          state = GAME_OVER;
        } else {
          reveal(cursorX, cursorY);
          if (checkWin()) {
            revealAll();
            state = WON;
          }
        }
      }
      moved = true;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (moved) requestUpdate();
}

void MinesweeperActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  int mt, mr, mb, ml;
  renderer.getOrientedViewableTRBL(&mt, &mr, &mb, &ml);
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight() - mb;

  renderer.clearScreen();

  if (state == DIFFICULTY_SELECT) {
    GUI.drawHeader(renderer, Rect{0, (metrics.topPadding + mt), pageWidth, metrics.headerHeight}, tr(STR_DIFFICULTY));

    const int contentTop = (metrics.topPadding + mt) + metrics.headerHeight + metrics.verticalSpacing;

    const char* labels[] = {nullptr, nullptr, nullptr};
    labels[0] = tr(STR_EASY);
    labels[1] = tr(STR_MEDIUM);
    labels[2] = tr(STR_HARD);

    const char* subs[] = {tr(STR_MINES_EASY_DESC), tr(STR_MINES_MEDIUM_DESC), tr(STR_MINES_HARD_DESC)};

    for (int i = 0; i < 3; ++i) {
      const int y = contentTop + i * 70;
      if (i == difficultyIndex)
        renderer.drawRect(metrics.contentSidePadding, y, pageWidth - 2 * metrics.contentSidePadding, 64);
      renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding + 8, y + 4, labels[i]);
      renderer.drawText(SMALL_FONT_ID, metrics.contentSidePadding + 8, y + 32, subs[i]);
    }

    const auto btnLabels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "^", "v");
    GUI.drawButtonHints(renderer, btnLabels.btn1, btnLabels.btn2, btnLabels.btn3, btnLabels.btn4);
    renderer.displayBuffer();
    return;
  }

  // Draw header with mine/flag info
  char headerInfo[96];
  snprintf(headerInfo, sizeof(headerInfo), "%s: %d  %s: %d", tr(STR_MINES), mineCount, tr(STR_FLAGS), flagCount);
  GUI.drawHeader(renderer, Rect{0, (metrics.topPadding + mt), pageWidth, metrics.headerHeight}, tr(STR_MINESWEEPER),
                 headerInfo);

  // Calculate cell size and grid position
  const int headerBottom = (metrics.topPadding + mt) + metrics.headerHeight + 4;
  const int hintsTop = pageHeight - metrics.buttonHintsHeight;
  const int availHeight = hintsTop - headerBottom - 4;
  const int availWidth = pageWidth - ml - mr - 2 * metrics.contentSidePadding;

  int cellSize = std::min(availWidth / cols, availHeight / rows);
  if (cellSize > 48) cellSize = 48;
  if (cellSize < 4) cellSize = 4;

  const int gridWidth = cellSize * cols;
  const int gridHeight = cellSize * rows;
  const int gridX = ml + (pageWidth - ml - mr - gridWidth) / 2;
  const int gridY = headerBottom + (availHeight - gridHeight) / 2;

  // Font metrics for centering text in cells
  const int fontH = renderer.getTextHeight(SMALL_FONT_ID);

  // Draw grid
  for (int x = 0; x < cols; x++) {
    for (int y = 0; y < rows; y++) {
      int px = gridX + x * cellSize;
      int py = gridY + y * cellSize;
      bool isCursor = (x == cursorX && y == cursorY && state == PLAYING);

      if (isRevealed(x, y)) {
        // Revealed cell
        if (isCursor) {
          // Cursor on revealed: white fill, 3px thick border, black content
          renderer.fillRect(px, py, cellSize, cellSize, false);
          renderer.drawRect(px, py, cellSize, cellSize, true);
          renderer.drawRect(px + 1, py + 1, cellSize - 2, cellSize - 2, true);
          renderer.drawRect(px + 2, py + 2, cellSize - 4, cellSize - 4, true);
          if (isMine(x, y)) {
            int cx = px + cellSize / 2;
            int cy = py + cellSize / 2;
            int mr = cellSize / 4;
            // Filled circle body
            for (int dy = -mr; dy <= mr; dy++) {
              int dx = 0;
              while ((dx + 1) * (dx + 1) + dy * dy <= mr * mr) dx++;
              if (dx > 0)
                renderer.fillRect(cx - dx, cy + dy, dx * 2 + 1, 1, true);
              else
                renderer.drawPixel(cx, cy + dy, true);
            }
            // 4 radiating lines (cross + X)
            renderer.drawLine(cx - mr - 2, cy, cx + mr + 2, cy);
            renderer.drawLine(cx, cy - mr - 2, cx, cy + mr + 2);
            renderer.drawLine(cx - mr, cy - mr, cx + mr, cy + mr);
            renderer.drawLine(cx - mr, cy + mr, cx + mr, cy - mr);
          } else {
            int val = getCellValue(x, y);
            if (val > 0) {
              char num[2] = {static_cast<char>('0' + val), 0};
              int tw = renderer.getTextWidth(SMALL_FONT_ID, num, EpdFontFamily::BOLD);
              renderer.drawText(SMALL_FONT_ID, px + (cellSize - tw) / 2, py + (cellSize - fontH) / 2, num, true,
                                EpdFontFamily::BOLD);
            }
          }
        } else {
          // Normal revealed
          renderer.fillRect(px, py, cellSize, cellSize, false);
          if (isMine(x, y)) {
            renderer.drawRect(px, py, cellSize, cellSize, true);
            int cx = px + cellSize / 2;
            int cy = py + cellSize / 2;
            int mr = cellSize / 4;
            // Filled circle body
            for (int dy = -mr; dy <= mr; dy++) {
              int dx = 0;
              while ((dx + 1) * (dx + 1) + dy * dy <= mr * mr) dx++;
              if (dx > 0)
                renderer.fillRect(cx - dx, cy + dy, dx * 2 + 1, 1, true);
              else
                renderer.drawPixel(cx, cy + dy, true);
            }
            // 4 radiating lines (cross + X)
            renderer.drawLine(cx - mr - 2, cy, cx + mr + 2, cy);
            renderer.drawLine(cx, cy - mr - 2, cx, cy + mr + 2);
            renderer.drawLine(cx - mr, cy - mr, cx + mr, cy + mr);
            renderer.drawLine(cx - mr, cy + mr, cx + mr, cy - mr);
          } else {
            int val = getCellValue(x, y);
            if (val > 0) {
              // Has number: draw border + number
              renderer.drawRect(px, py, cellSize, cellSize, true);
              char num[2] = {static_cast<char>('0' + val), 0};
              int tw = renderer.getTextWidth(SMALL_FONT_ID, num, EpdFontFamily::BOLD);
              renderer.drawText(SMALL_FONT_ID, px + (cellSize - tw) / 2, py + (cellSize - fontH) / 2, num, true,
                                EpdFontFamily::BOLD);
            }
            // Empty (val==0): no border, just blank white
          }
        }
      } else {
        // Unrevealed cell
        bool flagged = isFlagged(x, y);

        if (isCursor) {
          // Cursor on unrevealed: dithered fill, 3px thick border
          renderer.fillRect(px, py, cellSize, cellSize, false);
          fillDithered50(renderer, px, py, cellSize, cellSize);
          renderer.drawRect(px, py, cellSize, cellSize, true);
          renderer.drawRect(px + 1, py + 1, cellSize - 2, cellSize - 2, true);
          renderer.drawRect(px + 2, py + 2, cellSize - 4, cellSize - 4, true);
          if (flagged) {
            int cx = px + cellSize / 2;
            int cy = py + cellSize / 2;
            int fs = cellSize / 4;
            // Pole
            renderer.fillRect(cx + fs / 3, cy - fs, 2, fs * 2, true);
            // Triangle flag
            for (int dy = 0; dy < fs; dy++) {
              int fw = fs - dy;
              renderer.fillRect(cx + fs / 3 - fw, cy - fs + dy, fw, 1, true);
            }
            // Base
            renderer.fillRect(cx + fs / 3 - fs / 2, cy + fs, fs, 2, true);
          }
        } else {
          // Normal unrevealed: dithered fill with border
          renderer.fillRect(px, py, cellSize, cellSize, false);
          fillDithered50(renderer, px, py, cellSize, cellSize);
          renderer.drawRect(px, py, cellSize, cellSize, true);
          if (flagged) {
            int cx = px + cellSize / 2;
            int cy = py + cellSize / 2;
            int fs = cellSize / 4;
            // Pole
            renderer.fillRect(cx + fs / 3, cy - fs, 2, fs * 2, true);
            // Triangle flag
            for (int dy = 0; dy < fs; dy++) {
              int fw = fs - dy;
              renderer.fillRect(cx + fs / 3 - fw, cy - fs + dy, fw, 1, true);
            }
            // Base
            renderer.fillRect(cx + fs / 3 - fs / 2, cy + fs, fs, 2, true);
          }
        }
      }
    }
  }

  // Win/lose overlay
  if (state == WON) {
    GUI.drawPopup(renderer, tr(STR_YOU_WIN));
  } else if (state == GAME_OVER) {
    GUI.drawPopup(renderer, tr(STR_GAME_OVER));
  }

  if (state == PLAYING) {
    const auto labels = mappedInput.mapLabels(tr(STR_EXIT), tr(STR_REVEAL), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, tr(STR_GAME_HOLD_FLAG), labels.btn3, labels.btn4);
  } else {
    const auto labels = mappedInput.mapLabels(tr(STR_EXIT), tr(STR_NEW_GAME), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}
