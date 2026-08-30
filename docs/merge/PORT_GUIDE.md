# PORT GUIDE — how to bring an app in

The repeatable loop for every app. Worked example:
`src/activities/apps/calculator/` — its `PORT_NOTES.md` records both the
original audit and the re-audit that caught what the first pass missed.

## New app (from scratch)
```bash
python3 tools/port/new_app.py "Morse Code" --category tools
# -> src/activities/apps/morse_code/MorseCodeActivity.{h,cpp}  (ruleset-compliant skeleton)
# -> a budget stub appended to tools/port/app_budgets.yaml
```

Apps land in `src/activities/apps/<slug>/` because that is what the firmware
compiles (`build_src_filter = +<*>` covers `src/`). v1 staged them in a
top-level `apps-ported/` directory that nothing builds, so the gate never
actually ran on them: the Calculator's checklist was recorded green while five
of its strings were untranslatable. That directory is retired -- an app outside
`src/` is a file nobody compiles and nobody tests.

Registering the app in `AppRegistry.cpp` is what makes the rest work: the
launcher lists it, the simulator smoke test enters and renders it on every CI
run, and the budget gate expects its row.
Fill the TODOs, add the `STR_*` keys, measure the budget, register it, run the gate.

## Porting an app from biscuit
1. **Copy** biscuit's `src/activities/apps/<Name>Activity.{h,cpp}` →
   `src/activities/apps/<slug>/`. (biscuit shares CrossInk's `Activity` base, so
   most apps are near drop-in.) Record provenance in a `PORT_NOTES.md` beside it.
2. **Audit** against `PORT_CHECKLIST.md`. Fast first pass with grep:
   ```bash
   grep -nE "WiFi|BLE|esp_wifi|promiscuous|ESP_NOW" <file>   # radio?
   grep -nE "\bnew\b|malloc|std::vector|std::string|push_back" <file>   # alloc?
   grep -nE "IRAM_ATTR|portENTER_CRITICAL|attachInterrupt" <file>   # ISR?
   grep -noE '"[A-Z][a-z][^"]*"' <file>   # user strings needing tr()?
   ```
3. **Fix** every finding: cap collections, move allocation out of ISRs into a ring
   buffer, route the radio through `RADIO`, swap `std::string`→`char[]`, wrap
   strings in `tr()`. If it's a "security" app that's fake — fix it for real or cut it.
4. **Budget**: add/measure its entry in `app_budgets.yaml`; `budget_check.py` PASS.
5. **i18n**: add `STR_*` keys to `lib/I18n/translations/*.yaml`, regenerate.
6. **Register** it in `src/activities/apps/AppRegistry.cpp` under the right tile.
7. **Build + soak**: `pio run -e default && pio run -e simulator`; run it in the
   simulator; 10-min soak; confirm heap returns to baseline.
   Also run the host suite -- it now covers the merge foundations:
   `cmake -S test -B build/test && cmake --build build/test -j && ctest --test-dir build/test`.

   *Note for macOS:* ESP-IDF refuses to build from a path containing a space.
   If the checkout has one, build from a copy under a space-free path.
8. **PR** using the port template; tick the whole checklist.

## Update the ledger
Set the app's row in `docs/merge/PORT_LEDGER.md` in the same change that lands
it. A row saying `done` beside a tree that does not build the app is how v1's
records came apart.

## The order that keeps you sane
Port the **safe tier first** (no radio, small: calculator, clock, converters,
games), then the **recon tier** (radio, capped buffers), and gate the heavy
capture apps to `tier: psram`. See the phase roadmap.
