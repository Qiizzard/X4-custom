#pragma once
// Clock -- tools app. New port, not derived from biscuit or CrossPoint.
//
// v1 rejected a Clock app for pulling in <WiFi.h> directly to do its own NTP
// sync (docs/merge/PORT_LEDGER.md). This port is offline-only, per that same
// ledger row's suggested resolution: it only *reads* the RTC through the
// existing HalClock singleton (already used by BaseTheme's header clock and
// CrossPointSettings' sync bookkeeping) and never touches WiFi, NTP, or
// RadioManager itself. Actually syncing the RTC's wall-clock time remains a
// Settings-level concern elsewhere in the firmware; this app just displays
// whatever the RTC currently reports, and says so plainly (rule 21) when
// there is no RTC or the date has not been confirmed synced yet, rather than
// drawing a plausible-looking placeholder time.
#include "activities/Activity.h"

class ClockActivity final : public Activity {
 public:
  explicit ClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Clock", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // Repaint only when the displayed text actually changes (matches the
  // Countdown app's "repaint gated on the digit that changes" pattern) --
  // a full e-ink refresh every loop() tick for a screen that only needs to
  // change once a minute would be both slow and a battery cost.
  char lastTimeText[16] = {0};
  char lastDateText[24] = {0};
  bool lastAvailable = false;

  // Re-reads the RTC into timeText/dateText/available. Returns true if
  // anything displayed actually changed since the last call.
  bool refresh();

  char timeText[16] = {0};
  char dateText[24] = {0};
  bool available = false;
};
