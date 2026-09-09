#pragma once
// Adapted from biscuit, MIT, Copyright (c) 2025 Dave Allie.
#include <cstdint>
#include <memory>

#include "activities/Activity.h"

class EtchASketchActivity final : public Activity {
 public:
  explicit EtchASketchActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("EtchASketch", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // A logical 160x240 (or 240x160) drawing, not another display framebuffer.
  // Retained for this activity only; too large for the task stack.
  static constexpr size_t CANVAS_BYTES = 160 * 240 / 8;
  std::unique_ptr<uint8_t[]> canvas;
  int canvasW = 160, canvasH = 240;
  int cursorX = 0, cursorY = 0;
  bool penDown = true;
  enum class SaveStatus { None, Saved, Failed };
  SaveStatus saveStatus = SaveStatus::None;
  unsigned long lastMove = 0;
  void setPixel(int x, int y);
  bool getPixel(int x, int y) const;
  void saveToBmp();
};
