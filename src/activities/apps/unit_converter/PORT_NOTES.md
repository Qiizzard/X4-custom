# Port notes — Unit Converter

**Source:** biscuit `src/activities/apps/UnitConverterActivity.{h,cpp}` (MIT)
**Tier:** `c3` · **Category:** Tools

## Gate audit — what had to change

| Finding | Rule | Fix |
|---|---|---|
| `static const std::vector<Category>` of nested `std::vector<UnitDef>` | 1, 4 | Allocates on first use and then sits in DRAM for the life of the process. Rewritten as flat `constexpr` arrays: same data, in flash, costing no RAM. |
| `std::vector<std::string> conversionResults` rebuilt per recompute | 1, 2, 3 | Fixed `char rows[8][40]` in-object. Eight is the widest category (Length). |
| Rounded temperature constants (`0.5556`, `-17.7778`) | correctness | Close enough to look right and wrong enough to drift: C → F → C did not return the original value. Replaced with exact fractions (`5.0/9.0`, `-32.0*5.0/9.0`). The round-trip is now exact to 1e-9 across −100 … +100 °C, and that is a test. |
| Conversion maths buried in the activity | testing | Split into `UnitConversion.{h,cpp}` with 13 host tests (`test/unit_conversion/`), including an exhaustive round-trip over **every unit pair in every category** — that is what catches a mistyped scale factor without hand-writing a case per unit. |
| No `tr()` on user-facing strings | 18 | `STR_CONVERT_*` and `STR_APP_UNIT_CONVERTER`. |

## Corrections to the source data
- `Tonne` was labelled `Ton`, which is ambiguous (short/long/metric). It is the
  metric tonne; renamed.
- `Pound` and `Ounce` now use their exact definitions (`0.45359237`,
  `0.028349523125`) so `1 lb == 16 oz` holds exactly rather than to 5 places.
- `km/h` and `knot` written as exact fractions (`1/3.6`, `1852/3600`).

## Note on unit names
Unit and category names are **not** run through `tr()`. They are unit names, not
UI copy — "Kilometer" and "km" are the same in every locale this firmware ships,
and translating them would create 28 chances to introduce a wrong abbreviation.
The screen's chrome (title, button hints, prompts) is translated as usual.

## Verify on hardware
Tools → Unit Converter → Temperature → Celsius. Enter `100`: Fahrenheit must
read 212 and Kelvin 373.15. Enter `-40`: Fahrenheit must also read −40. Then
Data → Byte, enter `1`: TB should show a small non-zero value, not `0`.

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
