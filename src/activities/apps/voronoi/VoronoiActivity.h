#pragma once
#include <cstdint>

#include "activities/Activity.h"

class VoronoiActivity final : public Activity {
 public:
  VoronoiActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("Voronoi", renderer, input) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct Point {
    int x, y;
  };
  Point points[40] = {};
  // Fixed 6 KiB cache in the fallibly allocated activity, never on the task stack.
  uint8_t nearest[60][100] = {};
  int count = 20, width = 0, height = 0, cols = 0, rows = 0, step = 8;
  uint32_t rng = 1;
  uint32_t randomValue();
  void generate();
};
