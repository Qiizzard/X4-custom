# Port notes — Countdown

**Source:** biscuit `src/activities/apps/CountdownActivity.{h,cpp}` (MIT)
**Tier:** `c3` · **Category:** Tools

## Gate audit — what had to change

| Finding | Rule | Fix |
|---|---|---|
| No `tr()` on user-facing strings | 18 | `STR_COUNTDOWN_*` keys added. |
| Redrawn on every `loop()` pass while running | — | An e-ink full refresh per loop iteration is both slow and a real battery cost. The repaint is now driven by the *seconds* value changing, so it updates once a second and not otherwise. |
| Nothing kept the device awake while counting | — | `preventAutoSleep()` returns true **only** in the Running state. A timer that sleeps mid-count is useless; a picker screen that never sleeps is a battery bug. Gating on the state gets both right. |
| Presets as a runtime list | 4 | `static constexpr uint16_t kPresetMinutes[]` — flash, not DRAM. |

## Design note
Time is tracked as a remaining-milliseconds counter decremented by the measured
delta each loop, not as an end-timestamp compared against `millis()`. That makes
pause/resume exact and avoids the wraparound edge case at ~49 days of uptime.

## Verify on hardware
Tools → Countdown. Pick 1 min, Select to start. Confirm: the display ticks once
per second; Select pauses and resumes without losing time; the screen does not
sleep while running but does on the picker; it reaches "Time's up" and stops at
00:00 rather than counting negative. Back from a running timer returns to the
picker.

## Gate record (`PORT_CHECKLIST.md`)

| Section | Result |
|---|---|
| Memory — collections capped, `reserve()` before push loops, no `std::string` in hot paths, big tables `static const`, no bare `new`/`malloc` | PASS |
| Budget declared and `budget_check.py` PASS | PASS — `object_bytes` is a measured `sizeof()`, not an estimate |
| Radio & ISR | N/A — this app never touches WiFi/BLE and registers no callbacks |
| Lifecycle — buffers in `onEnter()`, freed reverse order in `onExit()`, no tasks, no open files | PASS |
| UI & i18n — every string via `tr()`, drawn through `UITheme`/`GUI`, no hardcoded 800/480, logical buttons only, registered in the launcher | PASS |
| Security | N/A — protects nothing, claims nothing |
| Builds `-e default` and `-e simulator` | PASS |
| Static analysis | Deferred to CI — PlatformIO's cppcheck is x86-only and cannot run on this machine (`docs/merge/ENVIRONMENT.md`) |
| Runs in the simulator without crashing | PASS — the smoke test enters and renders it on every run |
| Soak: 50 open/close, heap returns to baseline | **OUTSTANDING — needs hardware.** Tracked in `docs/merge/ACCEPTANCE.md` |
| Hardware note | See "Verify on hardware" above |
