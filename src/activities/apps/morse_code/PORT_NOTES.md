# Port notes — Morse Code

**Source:** biscuit `src/activities/apps/MorseCodeActivity.{h,cpp}` (MIT)
**Tier:** `c3` · **Category:** Tools

## Gate audit — what had to change

| Finding | Rule | Fix |
|---|---|---|
| `std::string result; result += ...` in the encode loop | 1, 3 | Rewritten to write into a caller-supplied `char*` with an explicit capacity. No allocation, no reallocation as the string grows. |
| Encode/decode buried in the activity | testing | Split into `MorseCode.{h,cpp}` — Arduino-free, UI-free, and covered by 15 host tests (`test/morse_code/`). A converter that is quietly wrong looks identical on screen to one that is right, so it gets tested rather than eyeballed. |
| Truncation on a full buffer | 16 | The original would silently produce a shortened message. A truncated Morse string is a *valid-looking but different* message, so `encode()`/`decode()` now return 0 and leave an empty buffer; the activity shows "Too long to convert". |
| Unknown code silently dropped on decode | 16 | Emits `?` instead, so a decode never quietly loses a character. |
| No `tr()` on user-facing strings | 18 | `STR_MORSE_*` keys added. |

## Kept from the original
The full A–Z / 0–9 international table and `/` as the word separator, verified
character-by-character in the round-trip test.

## Verify on hardware
Tools → Morse Code. Select opens the keyboard; type `SOS` and confirm it shows
`... --- ...`. Up/Down swaps direction; with the text carried over, decoding
that Morse must give `SOS` back. Type a very long sentence and confirm it says
"Too long to convert" rather than showing a shortened code.

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
| Static analysis | Deferred to CI — PlatformIO's cppcheck is x86-only and cannot run on this machine (`docs/merge/ENVIRONMENT.md`); the Arduino-free logic is additionally clean under `clang-tidy` |
| Runs in the simulator without crashing | PASS — the smoke test enters and renders it on every run |
| Soak: 50 open/close, heap returns to baseline | **OUTSTANDING — needs hardware.** Tracked in `docs/merge/ACCEPTANCE.md` |
| Hardware note | See "Verify on hardware" above |
