// Adapted from biscuit, MIT, Copyright (c) 2025 Dave Allie.
#pragma once
#include <memory>

#include "activities/Activity.h"

class FlashcardActivity final : public Activity {
 public:
  explicit FlashcardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Flashcard", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum State { DECK_SELECT, CARD_FRONT, CARD_BACK, STATS };
  State state = DECK_SELECT;

  struct Card {
    char front[128];
    char back[128];
    int correct;
    int wrong;
  };

  static constexpr int MAX_DECKS = 16;
  static constexpr int MAX_CARDS = 32;
  // One lifecycle allocation, about 10 KiB; too large for the task stack.
  struct DeckData {
    char names[MAX_DECKS][64]{};
    Card cards[MAX_CARDS]{};
    char line[260]{};
  };
  std::unique_ptr<DeckData> data;
  int deckCount = 0, cardCount = 0;
  bool loadError = false;
  bool scanLimited = false;
  int deckIndex = 0;
  int cardIndex = 0;

  static constexpr const char* DECK_DIR = "/crossink/flashcards";

  void scanDecks();
  bool loadDeck();
  void drawCardText(const char* text, int y, int bottom) const;
  void renderDeckSelect() const;
  void renderCardFront() const;
  void renderCardBack() const;
  void renderStats() const;
};
