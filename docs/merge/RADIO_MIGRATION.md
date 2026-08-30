# RADIO MIGRATION — getting every radio user behind `RADIO`

`RULESET.md` rule 7 says all WiFi/BLE goes through `RadioManager`. As of the
foundations landing, **new merge apps do; CrossInk's own network screens do
not.** This file is the standing record of that gap, so it stays a tracked
debt instead of quietly becoming the norm.

## Why it was not done in one pass

CrossInk predates this arbiter. Eight activities drive Arduino WiFi and ESP-NOW
directly, and they include the paths a bricked device is recovered through (OTA)
and the ones people use daily (OPDS, file transfer, nearby sync). Rewriting all
of them in the same change that *introduces* the arbiter would put the reader's
most load-bearing network paths through an untested layer, in a diff too large
to review honestly. `RULESET.md`'s own tie-breaker applies: stability beats
features.

## What protects the gap in the meantime

`RadioManager::acquire()` calls `foreignRadioActive()` — `WiFi.getMode() !=
WIFI_MODE_NULL` — and **refuses** rather than taking the antenna from a legacy
screen that is mid-download. So the failure mode is "the scanner says the radio
is busy", not "the OTA silently died". That is the right way round, but it is a
guard, not the fix: the legacy screens still leak state between themselves,
which is the class rule 7 exists to close.

## The call sites, in migration order

Safest first. Each row is one reviewed change with its own hardware check — not
a batch.

| # | Call site | Radio use | Mode | Risk |
|---|---|---|---|---|
| 1 | `settings/ClockSyncActivity.cpp` | NTP fetch | `WifiStation` | Low — short, self-contained, easy to re-test. |
| 2 | `settings/FontDownloadActivity.cpp` | HTTPS download | `WifiStation` | Low. |
| 3 | `reader/KOReaderSyncActivity.cpp` | Sync POST | `WifiStation` | Low. |
| 4 | `network/CrossPointWebServerActivity.cpp` | SoftAP + HTTP server | `WifiStation` (+AP mode: needs a new `RadioManager::Mode`) | Medium — AP mode is not modelled yet. |
| 5 | `network/NearbyStatsSyncActivity.cpp` | ESP-NOW | `EspNow` | Medium — verify against the existing `esp_now_init`/`deinit` pairing. |
| 6 | `reader/NearbyBookPositionSyncActivity.cpp` | ESP-NOW | `EspNow` | Medium — same, and it runs inside the reader. |
| 7 | `network/WifiSelectionActivity.cpp` | scan + associate + credential store | `WifiScan` → `WifiStation` | High — 18 call sites; it is the screen every other network feature depends on. |
| 8 | `settings/OtaUpdateActivity.cpp` | firmware download | `WifiStation` | **Highest — rule 23.** Do this one last and only with a tested stock-`update.bin` rollback to hand. |

## Rules for each migration

1. One call site per change. No batching, whatever the temptation.
2. `acquire()` in `onEnter()`, `shutdown()` in `onExit()`, unconditionally — rule 8.
3. `RadioManager` gains a mode only when a call site genuinely needs one (SoftAP
   at #4). Do not speculatively widen the enum.
4. Hardware check per row, named in the PR: the screen still works, **and** the
   next radio screen entered afterwards still works. The second half is the
   whole point.
5. #8 is not done until an OTA has been run and rolled back on a real device.

## Definition of done for this file

When all eight rows are migrated, `foreignRadioActive()` should never return
true. At that point make it an assertion in debug builds and delete this file.
