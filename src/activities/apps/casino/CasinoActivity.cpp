#include "CasinoActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"
namespace {
const char* lootName(unsigned item) {
  static constexpr StrId names[] = {
      StrId::STR_CASINO_ITEM_0,  StrId::STR_CASINO_ITEM_1,  StrId::STR_CASINO_ITEM_2,  StrId::STR_CASINO_ITEM_3,
      StrId::STR_CASINO_ITEM_4,  StrId::STR_CASINO_ITEM_5,  StrId::STR_CASINO_ITEM_6,  StrId::STR_CASINO_ITEM_7,
      StrId::STR_CASINO_ITEM_8,  StrId::STR_CASINO_ITEM_9,  StrId::STR_CASINO_ITEM_10, StrId::STR_CASINO_ITEM_11,
      StrId::STR_CASINO_ITEM_12, StrId::STR_CASINO_ITEM_13, StrId::STR_CASINO_ITEM_14, StrId::STR_CASINO_ITEM_15,
      StrId::STR_CASINO_ITEM_16, StrId::STR_CASINO_ITEM_17, StrId::STR_CASINO_ITEM_18, StrId::STR_CASINO_ITEM_19,
      StrId::STR_CASINO_ITEM_20, StrId::STR_CASINO_ITEM_21, StrId::STR_CASINO_ITEM_22, StrId::STR_CASINO_ITEM_23,
      StrId::STR_CASINO_ITEM_24, StrId::STR_CASINO_ITEM_25, StrId::STR_CASINO_ITEM_26, StrId::STR_CASINO_ITEM_27,
      StrId::STR_CASINO_ITEM_28, StrId::STR_CASINO_ITEM_29, StrId::STR_CASINO_ITEM_30, StrId::STR_CASINO_ITEM_31,
      StrId::STR_CASINO_ITEM_32, StrId::STR_CASINO_ITEM_33, StrId::STR_CASINO_ITEM_34, StrId::STR_CASINO_ITEM_35,
      StrId::STR_CASINO_ITEM_36, StrId::STR_CASINO_ITEM_37, StrId::STR_CASINO_ITEM_38, StrId::STR_CASINO_ITEM_39,
      StrId::STR_CASINO_ITEM_40, StrId::STR_CASINO_ITEM_41, StrId::STR_CASINO_ITEM_42, StrId::STR_CASINO_ITEM_43,
      StrId::STR_CASINO_ITEM_44, StrId::STR_CASINO_ITEM_45, StrId::STR_CASINO_ITEM_46, StrId::STR_CASINO_ITEM_47,
      StrId::STR_CASINO_ITEM_48, StrId::STR_CASINO_ITEM_49};
  return I18N.get(names[item]);
}
const char* lootRarity(unsigned item) {
  return item < 20   ? tr(STR_CASINO_COMMON)
         : item < 35 ? tr(STR_CASINO_RARE)
         : item < 45 ? tr(STR_CASINO_EPIC)
                     : tr(STR_CASINO_LEGENDARY);
}
}  // namespace
bool CasinoActivity::hasItem(unsigned item) const { return (collected[item / 8] & (1u << (item % 8))) != 0; }
void CasinoActivity::pullLoot() {
  if (choice == 2) {
    state = State::Collection;
    return;
  }
  const uint32_t cost = choice == 0 ? 100 : 450;
  insufficient = credits < cost;
  if (insufficient) return;
  credits -= cost;
  dirty = true;
  pullCount = choice == 0 ? 1 : 5;
  bool rare = false;
  for (unsigned i = 0; i < pullCount; ++i) {
    const unsigned roll = randomValue() % 100;
    unsigned start = 0, count = 20;
    if (roll < 3) {
      start = 45;
      count = 5;
    } else if (roll < 15) {
      start = 35;
      count = 10;
    } else if (roll < 40 || (i == 4 && !rare)) {
      start = 20;
      count = 15;
    }
    const unsigned item = start + randomValue() % count;
    pulls[i] = item;
    pullNew[i] = !hasItem(item);
    if (pullNew[i])
      collected[item / 8] |= 1u << (item % 8);
    else
      credits = std::min<uint32_t>(cap, credits + 25);
    if (item >= 20) rare = true;
  }
  state = State::Result;
}

uint32_t CasinoActivity::randomValue() {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return rng;
}
uint8_t CasinoActivity::drawCard() {
  if (position == 52) {
    for (unsigned i = 0; i < 52; ++i) deck[i] = i % 13 + 1;
    for (unsigned i = 51; i > 0; --i) std::swap(deck[i], deck[randomValue() % (i + 1)]);
    position = 0;
  }
  return deck[position++];
}
void CasinoActivity::onEnter() {
  Activity::onEnter();
  rng = millis() | 1u;
  loadProgress();
  requestUpdate();
}
void CasinoActivity::play() {
  if (mode == 5) {
    pullLoot();
    return;
  }
  const uint32_t bet = bets[betIndex];
  if (credits < bet) {
    insufficient = true;
    requestUpdate();
    return;
  }
  insufficient = false;
  pushed = false;
  credits -= bet;
  dirty = true;
  if (mode == 4) {
    // Classic reference machine: six equally weighted symbols, no powerups.
    static constexpr uint8_t payouts[] = {50, 20, 10, 8, 5, 3};
    for (auto& reel : reels) reel = randomValue() % 6;
    unsigned multiplier = 0;
    if (reels[0] == reels[1] && reels[1] == reels[2])
      multiplier = payouts[reels[0]];
    else if (reels[0] == reels[1] || reels[0] == reels[2] || reels[1] == reels[2])
      multiplier = 2;
    outcome = bet * multiplier;
    won = multiplier != 0;
    credits = uint32_t(std::min<uint64_t>(cap, uint64_t(credits) + outcome));
    state = State::Result;
    return;
  }
  if (mode == 3) {
    position = 52;  // Fresh single deck for each Blackjack round.
    playerCount = dealerCount = 2;
    player[0] = drawCard();
    dealer[0] = drawCard();
    player[1] = drawCard();
    dealer[1] = drawCard();
    state = State::Blackjack;
    if (handValue(player, 2) == 21 || handValue(dealer, 2) == 21) settleBlackjack(false);
    return;
  }
  if (mode == 1) {
    pot = bet;
    streak = 0;
    if (position > 40) position = 52;
    card = drawCard();
    state = State::HighLow;
    return;
  }
  unsigned multiplier = 2;
  if (mode == 0) {
    outcome = randomValue() % 2;
    won = outcome == choice;
  } else {
    outcome = randomValue() % 37;
    static constexpr uint8_t reds[] = {1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36};
    bool red = false;
    for (auto n : reds)
      if (n == outcome) red = true;
    switch (choice) {
      case 0:
        won = outcome && red;
        break;
      case 1:
        won = outcome && !red;
        break;
      case 2:
        won = outcome && outcome % 2;
        break;
      case 3:
        won = outcome && !(outcome % 2);
        break;
      case 4:
        won = outcome >= 1 && outcome <= 18;
        break;
      case 5:
        won = outcome >= 19;
        break;
      case 6:
      case 7:
      case 8:
        won = outcome >= 1 + (choice - 6) * 12 && outcome <= (choice - 5) * 12;
        multiplier = 3;
        break;
      default:
        won = outcome == number;
        multiplier = 36;
        break;
    }
  }
  if (won) credits = uint32_t(std::min<uint64_t>(cap, uint64_t(credits) + uint64_t(bet) * multiplier));
  state = State::Result;
}
void CasinoActivity::guess(bool higher) {
  outcome = drawCard();
  won = higher ? outcome >= card : outcome <= card;  // Reference counts equality as a win.
  if (won) {
    pot = uint32_t(std::min<uint64_t>(cap, uint64_t(pot) * 3 / 2));
    card = outcome;
    if (streak != UINT32_MAX) ++streak;
  } else {
    pot = 0;
    state = State::Result;
  }
}
void CasinoActivity::cashOut() {
  credits = uint32_t(std::min<uint64_t>(cap, uint64_t(credits) + pot));
  pot = 0;
  won = true;
  outcome = card;
  state = State::Result;
}
void CasinoActivity::loop() {
  const bool back = TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
                    mappedInput.wasPressed(MappedInputManager::Button::Back);
  if (back && state == State::Menu) {
    finish();
    return;
  }
  RenderLock lock(*this);
  if (back) {
    if (state == State::Blackjack)
      settleBlackjack(true);
    else if (state == State::HighLow)
      cashOut();
    else
      state = (state == State::Result || state == State::Collection) ? State::Bet : State::Menu;
    requestUpdate();
    return;
  }
  const bool left = mappedInput.wasPressed(MappedInputManager::Button::Left),
             right = mappedInput.wasPressed(MappedInputManager::Button::Right);
  const bool up = mappedInput.wasPressed(MappedInputManager::Button::Up),
             down = mappedInput.wasPressed(MappedInputManager::Button::Down);
  const bool confirm = mappedInput.wasPressed(MappedInputManager::Button::Confirm);
  bool changed = left || right || up || down || confirm;
  if (state == State::Menu) {
    if (left || right) {
      mode = (mode + (right ? 1 : 5)) % 6;
      choice = 0;
    }
    if (confirm) {
      state = State::Bet;
      insufficient = false;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::PageBack)) {
      state = State::Reset;
      changed = true;
    }
  } else if (state == State::Reset) {
    if (confirm) {
      credits = 1000;
      dirty = true;
      pot = 0;
      state = State::Menu;
    }
  } else if (state == State::Collection) {
    if (left || up) collectionIndex = (collectionIndex + 49) % 50;
    if (right || down) collectionIndex = (collectionIndex + 1) % 50;
    if (confirm) state = State::Bet;
  } else if (state == State::Result) {
    if (confirm) state = State::Bet;
  } else if (state == State::Blackjack) {
    if (right)
      settleBlackjack(true);
    else if (confirm) {
      if (playerCount < 12)
        player[playerCount++] = drawCard();
      else
        LOG_ERR("CASINO", "Player hand cap reached");
      if (handValue(player, playerCount) >= 21 || playerCount == 12) settleBlackjack(true);
    }
  } else if (state == State::HighLow) {
    if (confirm)
      cashOut();
    else if (up || down)
      guess(up);
  } else if (mode == 5) {
    if (left || up) choice = (choice + 2) % 3;
    if (right || down) choice = (choice + 1) % 3;
    if (confirm) play();
  } else {
    if (left && betIndex) --betIndex;
    if (right && betIndex < 6) ++betIndex;
    if ((up || down) && (mode == 0 || mode == 2)) {
      const unsigned choices = mode == 0 ? 2 : 10;
      choice = (choice + (down ? 1 : choices - 1)) % choices;
    }
    if (mode == 2 && choice == 9) {
      if (mappedInput.wasPressed(MappedInputManager::Button::PageBack)) {
        number = (number + 36) % 37;
        changed = true;
      }
      if (mappedInput.wasPressed(MappedInputManager::Button::PageForward)) {
        number = (number + 1) % 37;
        changed = true;
      }
    }
    if (confirm) play();
  }
  if (state == State::Menu && mappedInput.wasPressed(MappedInputManager::Button::PageForward)) {
    saveProgress();
    changed = true;
  }
  if (changed) requestUpdate();
}
void CasinoActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_CASINO), false);
  else
    GUI.drawHeader(renderer, header, tr(STR_APP_CASINO));
  const Rect area = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 8;
  int y = area.y + line;
  auto draw = [&](const char* s) {
    UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID, y, s);
    y += line;
  };
  char text[112];
  draw(tr(STR_CASINO_PLAY_ONLY));
  snprintf(text, sizeof(text), tr(STR_CASINO_CREDITS), static_cast<unsigned long>(credits));
  draw(text);
  draw(mode == 0   ? tr(STR_CASINO_COIN)
       : mode == 1 ? tr(STR_CASINO_HIGHLOW)
       : mode == 2 ? tr(STR_CASINO_ROULETTE)
       : mode == 3 ? tr(STR_CASINO_BLACKJACK)
       : mode == 4 ? tr(STR_CASINO_SLOTS)
                   : tr(STR_CASINO_LOOT));
  if (state == State::Menu) {
    draw(tr(STR_CASINO_SESSION));
    draw(saveError ? tr(STR_CASINO_SAVE_ERROR) : dirty ? tr(STR_CASINO_UNSAVED) : tr(STR_CASINO_SAVED));
    draw(tr(STR_CASINO_MENU));
  } else if (state == State::Reset)
    draw(tr(STR_CASINO_RESET));
  else if (mode == 5) {
    unsigned total = 0;
    for (unsigned i = 0; i < 50; ++i)
      if (hasItem(i)) ++total;
    snprintf(text, sizeof(text), tr(STR_CASINO_COLLECTION_COUNT), total);
    draw(text);
    if (state == State::Collection) {
      snprintf(text, sizeof(text), tr(STR_CASINO_ITEM_NUMBER), unsigned(collectionIndex) + 1);
      draw(text);
      draw(lootName(collectionIndex));
      draw(lootRarity(collectionIndex));
      draw(hasItem(collectionIndex) ? tr(STR_CASINO_OWNED) : tr(STR_CASINO_MISSING));
      draw(tr(STR_CASINO_COLLECTION_CONTROLS));
    } else if (state == State::Result) {
      for (unsigned i = 0; i < pullCount; ++i) {
        snprintf(text, sizeof(text), tr(STR_CASINO_PULL_RESULT), lootName(pulls[i]), lootRarity(pulls[i]),
                 pullNew[i] ? tr(STR_CASINO_NEW_ITEM) : tr(STR_CASINO_DUPLICATE));
        draw(text);
      }
      draw(tr(STR_CASINO_CONTINUE));
    } else {
      draw(choice == 0 ? tr(STR_CASINO_SINGLE) : choice == 1 ? tr(STR_CASINO_FIVE) : tr(STR_CASINO_COLLECTION));
      draw(tr(STR_CASINO_LOOT_ODDS));
      draw(tr(STR_CASINO_LOOT_GUARANTEE));
      draw(tr(STR_CASINO_LOOT_DUPLICATES));
      draw(tr(STR_CASINO_LOOT_CONTROLS));
      if (insufficient) draw(tr(STR_CASINO_INSUFFICIENT));
    }
  } else if (state == State::HighLow) {
    snprintf(text, sizeof(text), tr(STR_CASINO_POT), unsigned(card), static_cast<unsigned long>(pot),
             static_cast<unsigned long>(streak));
    draw(text);
    draw(tr(STR_CASINO_GUESS));
  } else if (state == State::Blackjack) {
    snprintf(text, sizeof(text), tr(STR_CASINO_BJ_HAND), handValue(player, playerCount), unsigned(dealer[0]));
    draw(text);
    size_t used = 0;
    for (unsigned i = 0; i < playerCount && used < sizeof(text); ++i) {
      const int n = snprintf(text + used, sizeof(text) - used, "%s%u", i ? " " : "", unsigned(player[i]));
      if (n < 0 || size_t(n) >= sizeof(text) - used) break;
      used += size_t(n);
    }
    draw(text);
    draw(tr(STR_CASINO_BJ_CONTROLS));
  } else if (state == State::Result) {
    if (mode == 4) {
      static constexpr StrId symbols[] = {StrId::STR_CASINO_SEVEN, StrId::STR_CASINO_BAR,  StrId::STR_CASINO_CHERRY,
                                          StrId::STR_CASINO_BELL,  StrId::STR_CASINO_STAR, StrId::STR_CASINO_DIAMOND};
      snprintf(text, sizeof(text), tr(STR_CASINO_REELS), I18N.get(symbols[reels[0]]), I18N.get(symbols[reels[1]]),
               I18N.get(symbols[reels[2]]));
      draw(text);
    }
    if (mode == 3) {
      snprintf(text, sizeof(text), tr(STR_CASINO_BJ_TOTALS), handValue(player, playerCount),
               handValue(dealer, dealerCount));
      draw(text);
    }
    draw(pushed ? tr(STR_CASINO_PUSH) : won ? tr(STR_CASINO_WIN) : tr(STR_CASINO_LOSS));
    snprintf(text, sizeof(text), mode == 4 ? tr(STR_CASINO_RETURN) : tr(STR_CASINO_OUTCOME), outcome);
    draw(text);
  } else {
    snprintf(text, sizeof(text), tr(STR_CASINO_BET), static_cast<unsigned long>(bets[betIndex]));
    draw(text);
    if (mode == 0) draw(choice ? tr(STR_CASINO_TAILS) : tr(STR_CASINO_HEADS));
    if (mode == 1) draw(tr(STR_CASINO_HIGHLOW_RULE));
    if (mode == 4) {
      draw(tr(STR_CASINO_SLOTS_RULE));
      draw(tr(STR_CASINO_SLOTS_PAY1));
      draw(tr(STR_CASINO_SLOTS_PAY2));
      draw(tr(STR_CASINO_SLOTS_LIMIT));
    }
    if (mode == 3) {
      draw(tr(STR_CASINO_BJ_RULE));
      draw(tr(STR_CASINO_BJ_LIMIT));
    }
    if (mode == 2) {
      static constexpr StrId choices[] = {StrId::STR_CASINO_RED,    StrId::STR_CASINO_BLACK,  StrId::STR_CASINO_ODD,
                                          StrId::STR_CASINO_EVEN,   StrId::STR_CASINO_LOW,    StrId::STR_CASINO_HIGH,
                                          StrId::STR_CASINO_DOZEN1, StrId::STR_CASINO_DOZEN2, StrId::STR_CASINO_DOZEN3,
                                          StrId::STR_CASINO_EXACT};
      draw(I18N.get(choices[choice]));
      if (choice == 9) {
        snprintf(text, sizeof(text), tr(STR_CASINO_NUMBER), number);
        draw(text);
      }
    }
    draw(tr(STR_CASINO_BET_CONTROLS));
    if (insufficient) draw(tr(STR_CASINO_INSUFFICIENT));
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

unsigned CasinoActivity::handValue(const uint8_t* hand, unsigned count) {
  unsigned value = 0, aces = 0;
  for (unsigned i = 0; i < count; ++i) {
    if (hand[i] == 1) {
      value += 11;
      ++aces;
    } else
      value += std::min<unsigned>(10, hand[i]);
  }
  while (value > 21 && aces) {
    value -= 10;
    --aces;
  }
  return value;
}
void CasinoActivity::settleBlackjack(bool drawDealer) {
  const unsigned p = handValue(player, playerCount);
  const bool natural = playerCount == 2 && p == 21;
  if (drawDealer && p <= 21 && !natural) {
    while (handValue(dealer, dealerCount) < 17 && dealerCount < 12) dealer[dealerCount++] = drawCard();
  }
  const unsigned d = handValue(dealer, dealerCount);
  const bool dealerNatural = dealerCount == 2 && d == 21;
  const uint64_t bet = bets[betIndex];
  uint64_t returned = 0;
  if (p > 21)
    returned = 0;
  else if (natural && !dealerNatural)
    returned = bet * 5 / 2;
  else if (dealerNatural && !natural)
    returned = 0;
  else if (d > 21 || p > d)
    returned = bet * 2;
  else if (p == d)
    returned = bet;
  won = returned > bet;
  pushed = returned == bet;
  credits = uint32_t(std::min<uint64_t>(cap, uint64_t(credits) + returned));
  outcome = p;
  state = State::Result;
}
