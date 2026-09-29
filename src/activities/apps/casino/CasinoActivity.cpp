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
  requestUpdate();
}
void CasinoActivity::play() {
  const uint32_t bet = bets[betIndex];
  if (credits < bet) {
    insufficient = true;
    requestUpdate();
    return;
  }
  insufficient = false;
  pushed = false;
  credits -= bet;
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
      state = state == State::Result ? State::Bet : State::Menu;
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
      mode = (mode + (right ? 1 : 4)) % 5;
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
      pot = 0;
      state = State::Menu;
    }
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
                   : tr(STR_CASINO_SLOTS));
  if (state == State::Menu) {
    draw(tr(STR_CASINO_SESSION));
    draw(tr(STR_CASINO_MENU));
  } else if (state == State::Reset)
    draw(tr(STR_CASINO_RESET));
  else if (state == State::HighLow) {
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
