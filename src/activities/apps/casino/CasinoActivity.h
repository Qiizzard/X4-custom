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
  enum class State { Menu, Bet, HighLow, Blackjack, Result, Reset, Collection, SlotOptions, SlotPayout };
  State state = State::Menu;
  unsigned mode = 0, betIndex = 1, choice = 0, number = 0, outcome = 0;
  uint32_t credits = 1000, pot = 0, rng = 1, streak = 0;
  static constexpr uint32_t cap = 1000000;
  static constexpr uint32_t bets[] = {10, 25, 50, 100, 250, 500, 1000};
  int8_t saveSlot = -1;
  uint32_t saveGeneration = 0;
  bool saveBlocked = false, saveError = false, dirty = true;
  void loadProgress();
  void saveProgress();
  uint8_t collected[7] = {}, pulls[5] = {}, pullCount = 0, collectionIndex = 0;
  bool pullNew[5] = {};
  bool hasItem(unsigned item) const;
  void pullLoot();
  uint8_t machine = 0, slotOption = 0, payoutIndex = 0, freeSpins = 0, held = 0;
  bool doubled = false, wild = false, reelsReady = false;
  uint8_t reels[3] = {};
  uint8_t deck[52] = {}, position = 52, card = 0;
  bool won = false, insufficient = false, pushed = false;
  uint8_t player[12] = {}, dealer[12] = {}, playerCount = 0, dealerCount = 0;
  static unsigned handValue(const uint8_t* hand, unsigned count);
  void settleBlackjack(bool drawDealer);
  uint32_t randomValue();
  uint8_t drawCard();
  void play();
  void guess(bool higher);
  void cashOut();
};
