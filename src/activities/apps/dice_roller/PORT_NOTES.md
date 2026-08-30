# Port notes — Dice Roller

**Source:** biscuit `src/activities/apps/DiceRollerActivity.{h,cpp}` (MIT)
**Tier:** `c3` · **Category:** Games

## Gate audit — what had to change

| Finding | Rule | Fix |
|---|---|---|
| `std::vector<int> diceResults` with `push_back` in a loop, no `reserve()`, re-filled on **every animation frame** | 1, 2 | `uint16_t results[6]`, in-object. The cap is 6 and known at compile time, so there is nothing to allocate and nothing to grow. |
| `esp_random()` via `<esp_random.h>` | build | Not available in the simulator, which would have made this app device-only. Replaced with an in-object xorshift32 seeded from `millis()`. A dice app needs an even distribution, not unpredictability against an adversary — and saying so in the header keeps it clearly distinct from `SecureStore`, where real randomness matters. |
| Hand-rolled 25%/50% dither fills and custom die-face drawing | 19 | Dropped. The app now draws through `TouchHeaderBackButton` / `GUI` / `drawCenteredText` like every other screen, so it inherits the shared typography and back-button grammar instead of inventing its own. |
| `constexpr int DiceRollerActivity::DIE_TYPES[];` out-of-line definition | — | Redundant since C++17; removed. |
| No `tr()` on any user-facing string | 18 | `STR_APP_DICE_ROLLER`, `STR_DICE_ROLL`, `STR_DICE_PICK_HINT`. |

## Kept from the original
Die types (d4, d6, d8, d10, d12, d20, d100), 1–6 dice, and the four-frame roll
animation — the animation is what makes it feel like a roll rather than a
number appearing, and it costs nothing but four repaints.

## Verify on hardware
Games → Dice Roller. Up/Down changes the die, Left/Right the count, Select
rolls. Confirm: the animation runs then settles; the total matches the dice
shown; Back from the result returns to the picker and Back again exits. Open
and close it 20× and watch free heap return to baseline — it allocates nothing,
so any drift is a bug elsewhere.

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
