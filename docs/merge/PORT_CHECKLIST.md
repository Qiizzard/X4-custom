# PORT CHECKLIST — the merge gate

No app enters the firmware until every box is ticked. This is the intake gate
referenced by `RULESET.md`; the PR template mirrors it. "N/A" is a valid answer
but must be justified in the PR.

**App:** ____________________  **Category:** __________  **Tier:** `c3` / `psram`
**Source:** biscuit `____` / new  **Ported by:** ______  **PR:** #____

### Memory
- [ ] Every collection is capped; no unbounded growth.
- [ ] `reserve()` called before any push loop.
- [ ] No `std::string` / `String` in hot paths (render/loop/callbacks).
- [ ] Large constant tables are `static const` (flash, not DRAM).
- [ ] Budget declared in `app_budgets.yaml`; `budget_check.py` **PASS**.
- [ ] No bare `new`/`malloc` for fallible allocations — `makeUniqueNoThrow`, nullptr-checked, `LOG_ERR` + fallback on OOM.

### Radio & ISR  *(skip only if the app never touches WiFi/BLE)*
- [ ] Radio only via `RADIO` / `RadioManager` — no direct `esp_wifi_*` / raw BLE.
- [ ] Acquire in `onEnter()`, `RADIO.shutdown()` in `onExit()`; symmetric.
- [ ] No allocation / sort / `std::string` inside any ISR or promiscuous callback.
- [ ] Callbacks write raw bytes to a fixed pre-allocated ring buffer only.
- [ ] ISR handlers `IRAM_ATTR`; ISR data in DRAM; no `xSemaphoreTake` in ISR.

### Lifecycle
- [ ] Long-lived buffers/tasks allocated in `onEnter()`, freed reverse-order in `onExit()`.
- [ ] All FreeRTOS tasks deleted before the activity is destroyed.
- [ ] All file handles closed in `onExit()`.

### UI & i18n
- [ ] Every user-facing string via `tr(STR_*)`; keys added to translations and regenerated.
- [ ] Drawn through `UITheme` / `GUI`; no hardcoded `800`/`480`; no app-specific fonts.
- [ ] Logical `MappedInputManager::Button::*` only; Back/Select/Nav grammar consistent.
- [ ] Registered in the correct launcher category.

### Security  *(only if the app claims to protect anything)*
- [ ] Real, reviewed crypto — no XOR-on-MAC, no TLS-validation-off, no dead duress paths.
- [ ] Threat-model note in the PR, or the feature is cut.

### Tests & verification
- [ ] Builds `-e default` (C3) **and** `-e simulator`.
- [ ] `pio check` static analysis: no new low/medium/high defects.
- [ ] Runs in the simulator; drives its main paths without crashing.
- [ ] Soak: opened/closed 50× and left running 10 min in the simulator or on device — free heap returns to baseline (no leak), no watchdog reset.
- [ ] Hardware note in the PR: device path tested, expected behaviour, any cache reset.

### Sign-off
- [ ] `CHANGELOG.md` entry added.
- [ ] Diff is scoped to this app — no unrelated cleanup.
