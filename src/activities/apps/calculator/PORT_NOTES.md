# Port notes — Calculator (pipeline proof)

First app through the gate. Chosen because it's genuinely representative and
already well-behaved: it proves the port pipeline end-to-end without heroics.

**Source:** biscuit `src/activities/apps/CalculatorActivity.{h,cpp}`
**Why it ports cleanly:** biscuit and CrossInk share the same `Activity` base
(`activities/Activity.h`), so this is a move + audit + i18n pass, not a rewrite.

## Gate audit (actual results)

Ran against `PORT_CHECKLIST.md`:

| Check | Result |
|---|---|
| Radio / WiFi / BLE | ✅ none — grep clean |
| Dynamic allocation (`new`/`malloc`/`vector`/`std::string`/`push_back`) | ✅ none — uses fixed `char displayStr[24]`, `expressionStr[40]` |
| Collections capped | ✅ `history[MAX_HISTORY=5]`, fixed |
| ISR / critical sections | ✅ none |
| File / SD persistence | ✅ none |
| Hardcoded screen dims | ✅ none — uses `renderer.getScreenWidth()` |
| Strings via `tr()` | ⚠️ **5 already localized; 5 literals remain** (see below) |
| Budget declared + `budget_check.py` PASS | ✅ Calculator: 0.6 KB peak, 199.9 KB headroom |

## The only code change required — i18n

Two lines carry hardcoded user-facing strings:

- **`CalculatorActivity.cpp:334`** — header title:
  ```cpp
  GUI.drawHeader(renderer, Rect{...}, "Calculator");
  // ->
  GUI.drawHeader(renderer, Rect{...}, tr(STR_APP_CALCULATOR));
  ```
- **`CalculatorActivity.cpp:345`** — button hints:
  ```cpp
  mappedInput.mapLabels("Back", "Press", "Nav", "Nav");
  // ->
  mappedInput.mapLabels(tr(STR_HINT_BACK), tr(STR_HINT_PRESS), tr(STR_HINT_NAV), tr(STR_HINT_NAV));
  ```
  (`STR_HINT_BACK/PRESS/NAV` almost certainly already exist — grep the i18n table
  before adding duplicates. Only `STR_APP_CALCULATOR` is likely new.)

## Remaining wiring (not code-audit, just integration)
- [ ] Add `STR_APP_CALCULATOR` to `lib/I18n/translations/*.yaml`, regenerate.
- [ ] Register `CalculatorActivity` in the **Tools** launcher menu.
- [ ] Confirm `sizeof(CalculatorActivity)` and refine `object_bytes` in the manifest.
- [ ] Build `-e default` + `-e simulator`; soak 10 min; tick the checklist; open the PR.

**Status:** audited, budgeted, i18n changes identified. Ready to compile in the
PlatformIO tree (can't be compiled in the planning workspace — no ESP-IDF there).

---

## Re-audit on the move into `src/` (v2)

The v1 notes above were written while this app lived in `apps-ported/`, a
top-level directory the firmware does not compile — so nothing it claimed had
ever been checked by a build. Re-auditing it on the way into `src/activities/apps/`
found the memory findings above to be accurate (no allocation, fixed buffers,
capped history — all correct), and one class of finding missed:

| Finding | Rule | Fix |
|---|---|---|
| Five user-facing strings hardcoded: the `"Calculator"` header title and the `"Back"` / `"Press"` / `"Nav"` / `"Nav"` button hints | 18 | Routed through `tr()`: `STR_APP_CALCULATOR`, `STR_BACK`, `STR_SELECT`, `STR_DIR_UP`, `STR_DIR_DOWN`. The app was untranslatable in all 28 languages. |
| Header drawn into a hand-built `Rect` from raw theme metrics | 19 | Now uses `TouchHeaderBackButton::headerRect()` with the touch/non-touch branch, so the back affordance and title placement match every other screen. |

`object_bytes` in `app_budgets.yaml` is now a measured `sizeof()` (584 B), not
the 640 B estimate.

**The lesson worth keeping:** a checklist ticked against code that is never
compiled is not a gate. Apps live in `src/` from here on.

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
