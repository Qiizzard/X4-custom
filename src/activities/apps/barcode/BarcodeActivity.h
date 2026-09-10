// Adapted from biscuit, MIT, Copyright (c) 2025 Dave Allie.
// See PORT_NOTES.md for bounded input and pattern corrections.
#pragma once
#include <cstddef>

#include "activities/Activity.h"

class BarcodeActivity final : public Activity {
 public:
  explicit BarcodeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Barcode", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum State { TYPE_SELECT, TEXT_INPUT, SHOWING };
  enum BarcodeType { CODE128, CODE39, EAN13 };
  static constexpr int BARCODE_TYPE_COUNT = 3;

  State state = TYPE_SELECT;
  BarcodeType barcodeType = CODE128;
  char inputText[49]{};
  size_t inputLength = 0;

  void launchKeyboard();
  void renderBarcode() const;
  void renderCode128() const;
  void renderCode39() const;
  void renderEan13() const;

  // Draw a single bar/space pattern segment starting at x
  void drawPattern(int x, int y, int h, const uint8_t pattern[6], int unitW) const;
};
