// Derived from biscuit, MIT license, Copyright (c) 2025 Dave Allie.
// Adapted for CrossInk; see PORT_NOTES.md.
#include "SudokuActivity.h"

#include <Arduino.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

// 25% gray (light) — every other pixel in checkerboard on even rows only
static void fillDithered25(GfxRenderer& r, int x, int y, int w, int h) {
  for (int dy = 0; dy < h; dy += 2)
    for (int dx = ((dy / 2) % 2); dx < w; dx += 2) r.drawPixel(x + dx, y + dy, true);
}

bool SudokuActivity::isValid(const uint8_t b[9][9], int row, int col, uint8_t num) const {
  for (int i = 0; i < 9; i++) {
    if (b[row][i] == num) return false;
    if (b[i][col] == num) return false;
  }
  int boxR = (row / 3) * 3, boxC = (col / 3) * 3;
  for (int r = boxR; r < boxR + 3; r++) {
    for (int c = boxC; c < boxC + 3; c++) {
      if (b[r][c] == num) return false;
    }
  }
  return true;
}

bool SudokuActivity::validateBoard(uint8_t b[9][9], bool requireFull) const {
  for (int r = 0; r < 9; ++r)
    for (int c = 0; c < 9; ++c) {
      const uint8_t value = b[r][c];
      if (value == 0) {
        if (requireFull) return false;
        continue;
      }
      b[r][c] = 0;
      const bool valid = value <= 9 && isValid(b, r, c, value);
      b[r][c] = value;
      if (!valid) return false;
    }
  return true;
}

void SudokuActivity::startSolving() {
  memcpy(solving, board, sizeof(board));
  if (!validateBoard(solving, false)) {
    state = NO_SOLUTION;
    requestUpdate();
    return;
  }
  emptyCount = 0;
  solveIndex = 0;
  for (int i = 0; i < 81; ++i)
    if (!solving[i / 9][i % 9]) emptyCells[emptyCount++] = i;
  state = SOLVING;
  requestUpdate();
}

void SudokuActivity::solveStep() {
  // At most 256 candidates per loop; Back is checked between batches.
  for (int budget = 0; budget < 256; ++budget) {
    if (solveIndex == emptyCount) {
      memcpy(board, solving, sizeof(board));
      state = SOLVED;
      requestUpdate();
      return;
    }
    if (solveIndex < 0) {
      state = NO_SOLUTION;
      requestUpdate();
      return;
    }
    const int cell = emptyCells[solveIndex], r = cell / 9, c = cell % 9;
    uint8_t candidate = solving[r][c] + 1;
    solving[r][c] = 0;
    while (candidate <= 9 && !isValid(solving, r, c, candidate)) ++candidate;
    if (candidate <= 9) {
      solving[r][c] = candidate;
      ++solveIndex;
    } else {
      --solveIndex;
    }
  }
}

void SudokuActivity::generatePuzzle() {
  // Permute a valid Latin-pattern solution. No recursive generation on the C3.
  // As in Biscuit, removing clues does not guarantee a unique solution.
  uint8_t digits[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  for (int i = 8; i > 0; --i) std::swap(digits[i], digits[nextRandom() % (i + 1)]);
  const int bandShift = nextRandom() % 3, rowShift = nextRandom() % 3;
  for (int r = 0; r < 9; ++r)
    for (int c = 0; c < 9; ++c) {
      const int sourceRow = ((r / 3 + bandShift) % 3) * 3 + (r + rowShift) % 3;
      board[r][c] = digits[(sourceRow * 3 + sourceRow / 3 + c) % 9];
    }
  uint8_t cells[81];
  for (int i = 0; i < 81; ++i) cells[i] = i;
  for (int i = 80; i > 0; --i) std::swap(cells[i], cells[nextRandom() % (i + 1)]);
  for (int i = 0; i < 51; ++i) board[cells[i] / 9][cells[i] % 9] = 0;
  for (int r = 0; r < 9; ++r)
    for (int c = 0; c < 9; ++c) fixed[r][c] = board[r][c] != 0;
}

void SudokuActivity::onEnter() {
  Activity::onEnter();
  rngState = static_cast<uint32_t>(millis()) | 1u;
  state = PLAYING;
  cursorX = 0;
  cursorY = 0;
  generatePuzzle();
  requestUpdate();
}

void SudokuActivity::loop() {
  if (state == SOLVING) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      state = PLAYING;
      requestUpdate();
      return;
    }
    solveStep();
    return;
  }
  if (state == SOLVED || state == NO_SOLUTION) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      state = PLAYING;
      generatePuzzle();
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
    }
    return;
  }

  bool moved = false;

  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    if (cursorY > 0) cursorY--;
    moved = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    if (cursorY < 8) cursorY++;
    moved = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    if (cursorX > 0) cursorX--;
    moved = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    if (cursorX < 8) cursorX++;
    moved = true;
  }

  // Confirm: cycle number (0->1->2->...->9->0) on non-fixed cells
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!fixed[cursorY][cursorX]) {
      if (mappedInput.getHeldTime() >= 500) {
        // Long press: auto-solve
        startSolving();
      } else {
        board[cursorY][cursorX] = (board[cursorY][cursorX] % 9) + 1;
      }
      moved = true;
    }
  }

  // PageForward: clear cell
  if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
    if (!fixed[cursorY][cursorX]) {
      board[cursorY][cursorX] = 0;
      moved = true;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (moved) {
    if (state == PLAYING && validateBoard(board, true)) state = SOLVED;
    requestUpdate();
  }
}

void SudokuActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  int mt, mr, mb, ml;
  renderer.getOrientedViewableTRBL(&mt, &mr, &mb, &ml);
  const auto pageHeight = renderer.getScreenHeight() - mb;

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, (metrics.topPadding + mt), pageWidth, metrics.headerHeight}, tr(STR_SUDOKU));

  const int headerBottom = (metrics.topPadding + mt) + metrics.headerHeight + 4;
  const int hintsTop = pageHeight - metrics.buttonHintsHeight;
  const int availHeight = hintsTop - headerBottom - 4;
  const int availWidth = pageWidth - ml - mr - 2 * metrics.contentSidePadding;

  int cellSize = std::min(availWidth / 9, availHeight / 9);
  if (cellSize > 50) cellSize = 50;

  const int gridSize = cellSize * 9;
  const int gridX = ml + (pageWidth - ml - mr - gridSize) / 2;
  const int gridY = headerBottom + (availHeight - gridSize) / 2;

  const int fontH = renderer.getTextHeight(SMALL_FONT_ID);

  // Draw cells
  for (int r = 0; r < 9; r++) {
    for (int c = 0; c < 9; c++) {
      int px = gridX + c * cellSize;
      int py = gridY + r * cellSize;
      bool isCursor = (c == cursorX && r == cursorY && state == PLAYING);

      if (isCursor) {
        // 3px thick cursor border
        renderer.drawRect(px, py, cellSize, cellSize, true);
        renderer.drawRect(px + 1, py + 1, cellSize - 2, cellSize - 2, true);
        renderer.drawRect(px + 2, py + 2, cellSize - 4, cellSize - 4, true);
      }

      // Fixed cells get light dithered background
      if (fixed[r][c] && !isCursor) {
        fillDithered25(renderer, px, py, cellSize, cellSize);
      }

      // Highlight cells with same number as cursor
      if (state == PLAYING && !isCursor && board[r][c] > 0 && board[cursorY][cursorX] > 0 &&
          board[r][c] == board[cursorY][cursorX]) {
        fillDithered25(renderer, px, py, cellSize, cellSize);
      }

      if (board[r][c] > 0) {
        char num[2] = {static_cast<char>('0' + board[r][c]), 0};
        EpdFontFamily::Style style = fixed[r][c] ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
        int tw = renderer.getTextWidth(SMALL_FONT_ID, num, style);
        renderer.drawText(SMALL_FONT_ID, px + (cellSize - tw) / 2, py + (cellSize - fontH) / 2, num, true, style);
      }
    }
  }

  // Draw grid lines
  for (int i = 0; i <= 9; i++) {
    int lx = gridX + i * cellSize;
    int ly = gridY + i * cellSize;
    bool thick = (i % 3 == 0);

    // Vertical line
    if (thick) {
      renderer.fillRect(lx - 1, gridY, 3, gridSize, true);
    } else {
      renderer.drawLine(lx, gridY, lx, gridY + gridSize);
    }

    // Horizontal line
    if (thick) {
      renderer.fillRect(gridX, ly - 1, gridSize, 3, true);
    } else {
      renderer.drawLine(gridX, ly, gridX + gridSize, ly);
    }
  }

  // Overlay for solved/no solution
  if (state == SOLVED) {
    GUI.drawPopup(renderer, tr(STR_SOLVED));
  } else if (state == NO_SOLUTION) {
    GUI.drawPopup(renderer, tr(STR_NO_SOLUTION));
  }

  if (state == SOLVING) {
    GUI.drawPopup(renderer, tr(STR_GAME_SOLVING));
    const auto labels = mappedInput.mapLabels(tr(STR_CANCEL), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (state == PLAYING) {
    const auto labels = mappedInput.mapLabels(tr(STR_EXIT), tr(STR_CONFIRM), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, tr(STR_GAME_HOLD_SOLVE), labels.btn3, labels.btn4);
  } else {
    const auto labels = mappedInput.mapLabels(tr(STR_EXIT), tr(STR_NEW_GAME), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}
