<!-- PR template for porting/adding an app. Fill it all in. See docs/merge/PORT_CHECKLIST.md -->

## App
- **Name / category / tier:**
- **Source:** biscuit `____` / new
- **What it does (one line):**

## Gate — all must be checked (justify any N/A)

**Memory**
- [ ] Collections capped; `reserve()` before push loops
- [ ] No `std::string`/`String` in hot paths; big const tables `static const`
- [ ] Budget in `app_budgets.yaml`; `budget_check.py` PASS
- [ ] No bare `new`/`malloc`; `makeUniqueNoThrow` + nullptr check

**Radio & ISR** (N/A if no radio)
- [ ] Radio only via `RADIO`; acquire `onEnter()` / shutdown `onExit()`
- [ ] No alloc/sort/string in ISR or callback; ring buffer only; `IRAM_ATTR`

**Lifecycle / UI / i18n**
- [ ] Buffers freed reverse-order, tasks deleted, files closed in `onExit()`
- [ ] All strings via `tr(STR_*)`; drawn through `UITheme`/`GUI`; no hardcoded dims
- [ ] Row added to `src/activities/apps/AppRegistry.cpp`, grouped with its tile
- [ ] `STR_APP_*` added to `lib/I18n/translations/english.yaml`
- [ ] Logical `Button::*` grammar: Back exits, Select confirms

**Security** (N/A if not a security feature)
- [ ] Real reviewed crypto; threat-model note — or feature cut

**Tests**
- [ ] Builds `-e default`, `-e sticky` and `-e simulator`
- [ ] `pio check` clean — or "deferred to CI" with the reason (see `ENVIRONMENT.md`)
- [ ] Host tests for any non-trivial logic (put it in its own Arduino-free file
      so it can be tested — see `morse_code/MorseCode.h`, `unit_converter/UnitConversion.h`)
- [ ] `run_simulator_smoke_test.py` passes — it walks the registry, so this
      covers the new app automatically
- [ ] 10-min soak, heap returns to baseline, no watchdog
- [ ] Hardware note: device path, expected behaviour, cache reset if any
- [ ] `CHANGELOG.md` updated; `PORT_LEDGER.md` row set; `PORT_NOTES.md` written
- [ ] Diff scoped to this app

**Flash** — the binding constraint; see `PORT_LEDGER.md`
- [ ] Firmware still builds without the low-headroom warning, **or** the PR says
      what the app cost and why it is worth it

## Hardware verification
<!-- device tested, what you saw, any cache reset needed -->
