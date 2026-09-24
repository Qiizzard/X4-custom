#pragma once
#include <cstdint>

#include "activities/Activity.h"

class MatrixRainActivity final : public Activity {
 public:
  explicit MatrixRainActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("MatrixRain", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  uint32_t rng = 1;
  uint32_t randomValue() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
  }
  bool paused = false;
  int originX = 0, originY = 0;
  int cols = 0;
  int rows = 0;
  static constexpr int CHAR_W = 10;
  static constexpr int CHAR_H = 14;

  static constexpr int MAX_COLS = 48;  // 480 / 10
  static constexpr int MAX_ROWS = 57;  // 800 / 14

  int dropHead[MAX_COLS] = {};
  int dropLength[MAX_COLS] = {};
  int dropSpeed[MAX_COLS] = {};
  bool dropActive[MAX_COLS] = {};

  char grid[MAX_ROWS * MAX_COLS] = {};

  unsigned long lastFrameMs = 0;
  static constexpr unsigned long FRAME_INTERVAL_MS = 800;
  uint32_t frameCount = 0;

  int speedLevel = 1;  // 0=slow (3000ms), 1=normal (2000ms), 2=fast (1000ms)
  static constexpr unsigned long SPEED_INTERVALS[] = {3000, 2000, 1000};

  int density = 2;  // 0=sparse, 1=normal, 2=dense

  void initColumns();
  void advanceFrame();
  void spawnDrop(int col);
  char randomChar();
  void drawChar(int col, int row, char c, int intensity);
};
