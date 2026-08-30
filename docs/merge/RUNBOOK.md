# RUNBOOK — build, flash, verify

The path from this repo to firmware running on your X4. Do the off-device gates
first; only flash when they're all green.

> The v1 revision of this file described a bootstrap-and-apply-a-kit flow, a
> `tools/qemu/run_smoke.py`, and a `pio run -e x4-pro` target. None of those
> exist in this repo. What follows is what actually runs.

---

## A. Off-device — must all pass before flashing

All of this runs on a laptop. See `ENVIRONMENT.md` for per-platform gotchas
(notably: **ESP-IDF refuses any project path containing a space**, and
PlatformIO's bundled cppcheck is x86-only).

```bash
# 1. Memory budget gate -- its own tests first, then the manifest
python3 tools/port/test_budget_check.py
python3 tools/port/budget_check.py

# 2. Host unit tests -- CrossInk's suite plus the merge's own
#    (RingBuffer, SecureStore, MorseCode, UnitConversion).
#    First run fetches googletest and mbedtls; later runs are seconds.
cmake -S test -B build/test -DCMAKE_BUILD_TYPE=Release
cmake --build build/test -j
ctest --test-dir build/test --output-on-failure

# 3. Formatting (CI enforces this; clang-format 21+)
./bin/clang-format-fix -g && git diff --exit-code

# 4. Static analysis
pio check -e default --fail-on-defect low --fail-on-defect medium --fail-on-defect high

# 5. Builds -- every target CI builds
pio run -e default      # ESP32-C3, X3/X4 -- the floor
pio run -e sticky       # ESP32-S3, Seeed Sticky
pio run -e simulator    # native SDL2

# 6. The click-through: boots, opens a book, turns pages, and enters every
#    registered app. Cheapest full regression there is -- run it every time.
python3 scripts/run_simulator_smoke_test.py
```

If any of these fail, fix before flashing.

### What the smoke test actually covers
It boots, walks Home → File Browser → Recent Books → Settings, opens the Tools
launcher and **every app in `AppRegistry`**, then opens an EPUB, turns pages,
and drives the reader menus. It fails on a crash, a hang, or a rejected render.
It does **not** check that anything is *correct* — that is what the host tests
and `ACCEPTANCE.md` are for.

---

## B. Flash (unlocked X4 only)

```bash
# 1. BACK UP FIRST. You are unlocked, so recovery is easy -- keep it that way.
python -m esptool --chip esp32c3 --port <PORT> read_flash 0x0 0x1000000 backup.bin
#    Keep a stock update.bin on the SD card as a second, independent rollback.

# 2. Flash (USB-C DATA cable, not a charge-only one)
pio run -e default -t upload
#    or flash .pio/build/default/firmware-x3-x4.bin via the CrossInk web flasher
```

**Before flashing anything that changes `partitions.csv`**, read rule 23 in
`RULESET.md` and the flash-budget section of `PORT_LEDGER.md`. A device running
the old table that takes an OTA built against a new one is the brick this
project promises not to ship. Repartitioning is its own change, with the
rollback tested first.

---

## C. Verify on device

Run **`ACCEPTANCE.md`** top to bottom. It is now in three parts:

- the original v1 sections, which mostly describe apps **not yet in the tree** —
  treat them as criteria for when those land, not a list to work through today;
- **v2 foundations** — the `SecureStore` KDF timing measurement (the one number
  that cannot be guessed) and the `RadioManager` release checks;
- **launcher and the first app batch** — heap-after-20-cycles, per-app
  correctness, and the button grammar.

---

## If a flash goes bad

Reset, hold power 3–5 s. If it bootloops, hold power + Up with an SD card
inserted to force SD recovery, or restore the backup:

```bash
python -m esptool --chip esp32c3 --port <PORT> write_flash 0x0 backup.bin
```

This path is load-bearing (rule 23). It is tested every release, and it is never
removed or "simplified".

---

## Cutting a release

1. Bump the version in `platformio.ini` (`[crossink] version`).
2. Finalise `CHANGELOG.md` — move `[Unreleased]` under the new version and date.
3. Confirm `PORT_LEDGER.md` and `GOVERNANCE.md`'s scope ledger match the tree.
   A row claiming `done` for something that does not build is how v1's records
   came apart.
4. Tag, attach the `-e default` and `-e sticky` `.bin`s.
5. Note the base (CrossInk) and the credits per `NOTICE`.
