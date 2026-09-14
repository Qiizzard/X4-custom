# RADIO MIGRATION — getting every radio user behind `RADIO`

`RULESET.md` rule 7 says all WiFi/BLE goes through `RadioManager`. As of the
foundations landing, **new merge apps do; CrossInk's own network screens do
not.** This file is the standing record of that gap, so it stays a tracked
debt instead of quietly becoming the norm.

## Active P5 implementation (2026-09-14)

The user-approved active queue supersedes the historical one-site/no-batching
cadence below: related source migrations share one compile check, with runtime
and hardware checks deferred to V1. ClockSync and FontDownload now acquire a
named station hold before opening their existing WiFi picker, and release only
their own hold at exit. RadioManager's owner-checked shutdown cannot release a
different owner; station-status reads also require the matching owner. No new
radio mode or dynamic RadioManager storage was added. Picker allocations now
fail cleanly. Font-change restart happens after releasing the owned session.

These two activities no longer directly start/stop/query Arduino WiFi. Busy
managed or foreign legacy sessions are left intact and reported unavailable;
ClockSync no longer silently borrows an already-connected foreign session.
The normal radio-off → picker → operation → exit path remains the intended
flow. Picker success leaves the connection for its parent; cancellation can
stop the driver, followed by the parent's manager cleanup. Startup failures
cannot trigger font-network retries without a hold. Simulator refuses radio.

Status: sites 1–2 source integrated, **wip/unverified**; sites 3–8 remain pending.
The shared WifiSelection child still uses direct SDK calls, so this is not a
claim that every network operation is already behind the manager. Its later
migration must preserve the parent hold through scanning/association and cancel
cleanup. Next group starts with KOReaderSync, preserving its TLS/restart path.
No OTA/recovery code or shipping partitions changed.

V1/device checks: on X3/X4, Settings > System > Device > Clock sync, connect,
sync, Back; then enter font downloads and cancel/complete a download, exit, and
open another network screen. Expect clock_sync/font_download acquisition and
release logs, no hold after exit, and normal font-change restart. Cover wrong
credentials, dropped connection, picker cancel, allocation failure, global Home,
foreign-radio denial and attempted release by a different owner. Check C3 free
heap, largest block and task stack high-water marks. No cache reset is required
for radio ownership. No physical or runtime checks have been performed here.

Compile evidence: C3 default PASS (145.447s), `/tmp/x4-p5a-build.log`,
2026-09-14. Firmware 6,391,744 bytes, stock OTA free 161,856 bytes, reserve
warning remains. No runtime/network/host/simulator/hardware tests performed.

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
