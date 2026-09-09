// Derived from biscuit, MIT license, Copyright (c) 2025 Dave Allie.
// Adapted for CrossInk; see PORT_NOTES.md.
#pragma once
#include <cstdint>

#include "activities/Activity.h"

class SudokuActivity final : public Activity {
 public:
  explicit SudokuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Sudoku", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  // Game randomness only, never suitable for secrets.
  uint32_t rngState = 1;
  uint32_t nextRandom() {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return rngState;
  }

  enum State { PLAYING, SOLVING, SOLVED, NO_SOLUTION };

  uint8_t board[9][9]{};  // current board (0 = empty)
  bool fixed[9][9]{};     // true = given clue, not editable
  int cursorX = 0;
  int cursorY = 0;
  State state = PLAYING;

  void generatePuzzle();
  uint8_t solving[9][9]{};
  uint8_t emptyCells[81]{};
  int emptyCount = 0, solveIndex = 0;
  void startSolving();
  void solveStep();
  bool validateBoard(uint8_t b[9][9], bool requireFull) const;
  bool isValid(const uint8_t b[9][9], int row, int col, uint8_t num) const;
};
