# Port notes — Game of Life

**Source:** New port, not derived from biscuit or CrossPoint (Conway's Game
of Life is common enough, and simple enough, that this was written fresh
against `docs/merge/RULESET.md` rather than adapted line-by-line from a
biscuit implementation this repo doesn't vendor a copy of).
**Tier:** `c3` · **Category:** Games

## Design notes

- Fixed 64x128 bit-packed board (rules 1/2: nothing here grows). Two
  ping-pong buffers of 1,024 bytes each are allocated in `onEnter()` and
  released in `onExit()` (2,048 bytes total, matching the `peak_heap_bytes`
  already declared for this app in `tools/port/app_budgets.yaml` before this
  port landed). Allocation failure is checked (rule 6) and shown on screen
  (`STR_GOL_OOM`) rather than dereferencing a null buffer.
- Toroidal wraparound (the board has no edges) so a glider or other moving
  pattern doesn't just die against a boundary.
- Advancing a generation is an explicit user action (Confirm), not a timer.
  An e-ink screen redrawing an entire ~8K-cell board on its own clock would
  be exactly the battery-cost mistake flagged elsewhere in
  `docs/merge/PORT_LEDGER.md` for Matrix Rain and Voronoi — this app never
  repaints unless the player asks it to (step or restart).
- Random reseeding (Up) uses the same xorshift32-seeded-from-`millis()`
  approach `DiceRollerActivity` already documents for itself: an
  even-looking distribution is all a starting pattern needs, this is not a
  security-sensitive use of randomness, so it deliberately does not reach
  for the heavier `mbedtls_ctr_drbg` primitive `OtpGeneratorActivity` and
  `SecureStore` use for actual secrets.
- Cell size on screen is computed from the live device's screen dimensions
  (`min(pageWidth / 64, boardHeight / 128)`, floored to at least 1px) rather
  than assuming a fixed resolution, so the board displays correctly on any
  supported panel size without a hardcoded 800/480 (rule per the UI/i18n
  checklist item other apps' `PORT_CHECKLIST.md` sections call out).

## What this session's simulator run actually verified

`scripts/run_simulator_smoke_test.py`'s registry-driven walk enters, renders,
and exits GameOfLife without crashing — which does exercise `onEnter()`'s
allocation, `seedRandom()`'s initial board fill, and one full `render()` pass
drawing whatever cells that seed produced (a meaningful check on its own: an
early version of this file was rendering the board with `1u << (wx % 8)`-style
bit math, and any off-by-one there would show up either as a crash or as a
visibly wrong-looking board on the very first render, not just on `step()`).
It does **not** exercise `step()` (Confirm) or the manual-reseed path (Up) —
the smoke test's app walk enters and renders each app once, it does not drive
each app's own control scheme. Hardware/manual verification is still needed
for: Confirm actually advancing one generation per press with correct
Conway rules, Up reseeding rather than crashing or leaking the old board's
memory, and the on-screen `Gen`/`Alive` counters staying in sync with what
the board visibly shows.

## Verify on hardware

Tools → Game of Life. Confirm Up produces a new random board each time (not
the same pattern twice), Confirm steps exactly one generation per press
(watch a small known-periodic pattern, e.g. a blinker, if the initial random
seed happens to produce one, or eyeball general population stabilizing over
several steps), and Back exits cleanly. Open and close the app repeatedly —
the board buffers are freed in `onExit()`, so free heap should return to
baseline every time.

## Gate record — audited 2026-09-05

**Status: wip.** Previous ~100-second holds were not the required ten-minute
soak. Older narrative above describes prior work, not fresh gate evidence.
The dated session report is the current verification record.

| Check | Current result |
|---|---|
| Registry and source | Present; simulator smoke walks entry/render/exit |
| Activity object | 252 bytes measured from C3 factory allocation; manifest updated |
| RAM budget | Declared-budget checker passes; runtime peak still needs validation where noted below |
| Builds | See `docs/merge/SESSION_REPORT_2026-09-05.md` for current C3/simulator results |
| Static analysis | Deferred to CI on Apple Silicon; not claimed run locally |
| Soak | Full 50-cycle/600000-ms entry-screen test recorded in session report; does not cover all interactions |
| Remaining checks | Confirm advances exactly one generation; Up reseeds; Gen/Alive counters match; Back exits. Check known Conway patterns and edge wrap. |
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
