#pragma once
// Game of Life -- games app. New port, not derived from biscuit or CrossPoint.
// Conway's Game of Life is simple enough, and different enough from
// biscuit's actual implementation choices, that this is written fresh
// against docs/merge/RULESET.md rather than adapted line-by-line.
#include <cstdint>
#include <memory>

#include "activities/Activity.h"

class GameOfLifeActivity final : public Activity {
 public:
  explicit GameOfLifeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GameOfLife", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
#ifdef SIMULATOR
  friend bool verifySimulatorGameOfLifeRules(GfxRenderer&, MappedInputManager&);
#endif
  // Fixed, compile-time grid size (rules 1/2: nothing here grows). Bit-packed,
  // 1 bit per cell, row-major, so the whole board is small and cheap to flip
  // between two ping-pong buffers each generation. 64 x 128 = 8192 cells =
  // 1024 bytes/buffer; two buffers = 2048 bytes, matching the budget already
  // declared for this app in tools/port/app_budgets.yaml.
  static constexpr int kGridWidth = 64;
  static constexpr int kGridHeight = 128;
  static constexpr size_t kRowBytes = kGridWidth / 8;
  static constexpr size_t kBufferBytes = kRowBytes * kGridHeight;

  // Allocated in onEnter(), released in onExit() (same lifecycle discipline as
  // OtpGeneratorActivity's mbedtls contexts) rather than living in the object
  // permanently -- this app is not on screen most of the time the firmware is
  // running, so there is no reason to hold 2 KB for it while it's closed.
  std::unique_ptr<uint8_t[]> current;
  std::unique_ptr<uint8_t[]> next;
  bool boardReady = false;

  // xorshift32 seeded from the clock -- an even-looking random starting
  // pattern is all this needs, not unpredictability against an adversary
  // (same reasoning DiceRollerActivity already documents for itself).
  uint32_t rngState = 1;
  uint32_t nextRandom();

  uint32_t generation = 0;
  uint32_t liveCount = 0;

  bool cellAt(const uint8_t* board, int x, int y) const;
  void setCellAt(uint8_t* board, int x, int y, bool alive) const;
  void seedRandom();
  void step();  // advance one generation: current -> next, then swap
};
