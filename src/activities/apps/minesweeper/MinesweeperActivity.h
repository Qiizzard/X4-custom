// Derived from biscuit, MIT license, Copyright (c) 2025 Dave Allie.
// Adapted for CrossInk; see PORT_NOTES.md.
#pragma once
#include <cstdint>

#include "activities/Activity.h"

class MinesweeperActivity final : public Activity {
 public:
  explicit MinesweeperActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Minesweeper", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  // Game randomness only; never use this generator for secrets.
  uint32_t rngState = 1;
  uint32_t nextRandom() {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return rngState;
  }

  enum State { DIFFICULTY_SELECT, PLAYING, WON, GAME_OVER };

  static constexpr int MAX_COLS = 10;
  static constexpr int MAX_ROWS = 16;

  // Cell flags packed in upper bits
  static constexpr uint8_t MINE = 9;
  static constexpr uint8_t REVEALED = 0x10;
  static constexpr uint8_t FLAGGED = 0x20;

  State state = DIFFICULTY_SELECT;
  int difficultyIndex = 1;  // default Medium

  int cols = 10;
  int rows = 16;
  int mineCount = 25;
  uint8_t grid[MAX_COLS][MAX_ROWS]{};

  int cursorX = 0;
  int cursorY = 0;
  int flagCount = 0;
  bool firstReveal = true;

  void initGame();
  void placeMines(int safeX, int safeY);
  void calculateNumbers();
  void reveal(int x, int y);
  void revealAll();
  bool checkWin() const;
  int countAdjacentMines(int x, int y) const;

  int getCellValue(int x, int y) const { return grid[x][y] & 0x0F; }
  bool isRevealed(int x, int y) const { return grid[x][y] & REVEALED; }
  bool isFlagged(int x, int y) const { return grid[x][y] & FLAGGED; }
  bool isMine(int x, int y) const { return getCellValue(x, y) == MINE; }
};
