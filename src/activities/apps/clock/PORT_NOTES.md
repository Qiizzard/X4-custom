# Port notes — Clock

**Source:** New port, not derived from biscuit or CrossPoint.
**Tier:** `c3` · **Category:** Tools

## Why this was `blocked`, and how that's resolved

`docs/merge/PORT_LEDGER.md` recorded: "v1 rejected it for pulling in
`<WiFi.h>` for NTP. Ship offline-only against the RTC, or gate NTP behind
`RADIO`." This port takes the first option: it only reads the existing
`HalClock` singleton (`lib/hal/HalClock.h` on `-e default`, `lib/HalClockSim`
on `-e simulator`) — `isAvailable()`, `formatTime()`, `formatDate()` — and
never includes `<WiFi.h>` or calls `syncFromNTP()`/`syncSystemTimeFromNTP()`
itself. Actually syncing the RTC's wall-clock time is already handled
elsewhere (`ClockSyncActivity`, `CrossPointSettings`); this app is a
read-only display, so it never needed the radio in the first place.

## Design notes

- Repaint is gated on the formatted time/date text actually changing
  (`refresh()` compares against the last-shown strings), mirroring
  `CountdownActivity`'s "repaint on the digit that changes" pattern — a
  clock face that redraws on every `loop()` tick would be both pointless
  (the display only needs to change once a minute) and a battery cost on
  e-ink.
- `!halClock.isAvailable()` draws an explicit "No RTC available on this
  device" message (rule 21) rather than a static or zeroed placeholder time.
  The simulator's `HalClockSim::isAvailable()` always returns `false`, so the
  simulator smoke test exercises exactly this fallback path, not the RTC
  happy path — that still needs hardware (see below).
- The date line is additionally gated on `SETTINGS.clockDateHasBeenSynced`,
  matching `HeaderDate.cpp`'s existing caution: don't draw a calendar date
  before the RTC's date specifically has been confirmed synced, even if the
  time-of-day looks plausible. Falls back to "Date not yet synced" rather
  than a possibly-wrong date.
- 12/24-hour format and date format/separator are read from `SETTINGS`
  (`clockFormat`, `dateFormat`, `dateSeparator`, `clockUtcOffsetQ`) — the
  same settings the header clock and `HeaderDate` already use, so this app's
  display matches whatever the rest of the firmware is already showing.

## Verify on hardware

Tools → Clock, on a device with the RTC populated. Confirm the time matches
wall-clock time (accounting for `SETTINGS.clockUtcOffsetQ`), the 12/24-hour
and date-format settings are respected, and the display does not repaint
every frame (watch for e-ink flicker) — only when the minute changes. Then
confirm the "No RTC available" and "Date not yet synced" fallback strings
actually appear on a freshly-flashed device before the clock has ever been
synced, since the simulator can only prove those strings compile and render,
not that they show up at the right real-world moment.

## Gate record — audited 2026-09-05

**Status: wip.** Previous ~100-second holds were not the required ten-minute
soak. Older narrative above describes prior work, not fresh gate evidence.
The dated session report is the current verification record.

| Check | Current result |
|---|---|
| Registry and source | Present; simulator smoke walks entry/render/exit |
| Activity object | 312 bytes measured from C3 factory allocation; manifest updated |
| RAM budget | Declared-budget checker passes; runtime peak still needs validation where noted below |
| Builds | See `docs/merge/SESSION_REPORT_2026-09-05.md` for current C3/simulator results |
| Static analysis | Deferred to CI on Apple Silicon; not claimed run locally |
| Soak | Full 50-cycle/600000-ms entry-screen test recorded in session report; does not cover all interactions |
| Remaining checks | Populated RTC: advancing time, synced date, timezone/date format and no-RTC fallback on real X4. |
| Hardware | Use the path described above; no cache reset for these apps |

The QR/Cipher keyboard object uses `makeUniqueNoThrow` with a checked failure
path where applicable. The shared keyboard still owns dynamic strings;
checking its outer allocation is not a guarantee against every possible OOM
inside shared UI infrastructure. No new resident buffer was added by the audit.

### Full lifecycle soak verified 2026-09-06

50 open/close cycles and a full 600000-ms hold PASS; final Home cleanup
passed within the 4096-byte tolerance. Entry-screen coverage only. See
`docs/merge/verification/BATCH_1_SOAK_2026-09-06.md` for measurements and
coverage limits. Other listed checks remain open; status stays wip.

### Batch 4 — 2026-09-08 UTC

Simulator resource review and button checks recorded in
`docs/merge/verification/BATCH_4_BASIC_APPS_2026-09-08.md`. Game of Life
known-pattern/wrap/counter tests pass; Clock/OTP still exercise unavailable
capability paths only. Physical and C3 resource gates remain open; wip.
