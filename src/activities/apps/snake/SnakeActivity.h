// Derived from biscuit, MIT license, Copyright (c) 2025 Dave Allie.
// Adapted for CrossInk; see PORT_NOTES.md.
#pragma once

#include <cstdint>
#include <memory>

#include "activities/Activity.h"

class SnakeActivity final : public Activity {
 public:
  explicit SnakeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Snake", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
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

  enum State { PLAYING, GAME_OVER };

  State state = PLAYING;

  // Grid
  static constexpr int CELL_SIZE = 12;
  int gridW = 0;
  int gridH = 0;
  int offsetX = 0;
  int offsetY = 0;

  // Snake
  struct Point {
    int16_t x, y;
  };
  static constexpr int MAX_CELLS = 32 * 48;
  std::unique_ptr<Point[]> snake;
  int snakeLength = 0;
  int dirX = 1, dirY = 0;          // current direction
  int nextDirX = 1, nextDirY = 0;  // buffered next direction

  // Food
  Point food;

  // Timing
  unsigned long lastStepMs = 0;
  static constexpr unsigned long STEP_INTERVAL_MS = 300;

  // Score
  int score = 0;

  void initGame();
  void step();
  void spawnFood();
  bool isSnakeAt(int x, int y) const;

  void renderPlaying() const;
  void renderGameOver() const;
};
