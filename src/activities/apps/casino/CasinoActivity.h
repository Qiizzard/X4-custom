#pragma once
#include <cstdint>

#include "activities/Activity.h"
class CasinoActivity final : public Activity {
 public:
  CasinoActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("Casino", renderer, input) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class State { Menu, Bet, HighLow, Result, Reset };
  State state = State::Menu;
  unsigned mode = 0, betIndex = 1, choice = 0, number = 0, outcome = 0;
  uint32_t credits = 1000, pot = 0, rng = 1, streak = 0;
  static constexpr uint32_t cap = 1000000;
  static constexpr uint32_t bets[] = {10, 25, 50, 100, 250, 500, 1000};
  uint8_t deck[52] = {}, position = 52, card = 0;
  bool won = false, insufficient = false;
  uint32_t randomValue();
  uint8_t drawCard();
  void play();
  void guess(bool higher);
  void cashOut();
};
