# Batch 4 — Clock, OTP and Game of Life, 2026-09-08 UTC

All three apps remain **wip**; this batch closes off-device checks only.

## Verification

Simulator build PASS in 51.308 seconds. Final smoke PASS: exit 0; explicit success marker at 53656 ms.
Commands: `pio run -e simulator`, then
`python3 scripts/run_simulator_smoke_test.py --no-build` (90-second timeout).
Logs: /tmp/x4-batch4-build.log and /tmp/x4-batch4-smoke.log.

New isolated Game of Life assertions call the actual activity's private
step/seed methods through a SIMULATOR-only friend function. They check:

- A 2x2 block stays at the same four coordinates with generation 1.
- A three-cell blinker across x=63/0 rotates across y=127/0 and returns after
  two steps; exact coordinates, population and generation are asserted.
- Restart resets generation and the recorded live count matches a recount
  of the board bits (nonempty fixed-seed test).

The isolated test activity and two boards are heap-owned with checked
nothrow allocation and automatic destruction on every return. It does not
call onEnter or modify the active activity. Its deterministic game seed has
no relationship to OTP entropy. The test hook does not exist in firmware
builds; no production Game of Life algorithm changed.

Additional button scripts assert the active screen after Game of Life
Confirm/Up/Confirm, OTP Confirm/Down/Up/Confirm, and Back to Home from each
of those apps and Clock. These establish button routing and successful
render requests. They do not read displayed pixels or counters from the
active screen. The isolated Game of Life assertions separately establish
step/restart method behavior. Allocation-failure injection remains untested.

The existing batch 1 50-cycle/ten-minute entry-screen soak evidence remains
applicable; it was not repeated. Prior QR/Cipher and reader smoke checks
remain in this run. No new C3/S3 build or hardware result is claimed for
these simulator-only changes.

## Resource and capability review

Clock holds four fixed character buffers (16+24+16+24 = 80 bytes). It owns no
additional dynamic buffer, task, file or radio session. Refresh requests are
gated on changed availability/time/date text. Standard simulator HalClock
sets unavailable unless an X3/Sticky/X4-Pro profile is selected
(.pio/libdeps/simulator/simulator/src/HalClock.cpp:20). This run used the
standard simulator profile; no RTC was fabricated. Populated date/time,
timezone offsets, formats and real refresh behavior remain physical checks.

Game of Life allocates two 1024-byte bit-packed boards in onEnter, reuses
and swaps them for each step, and releases next then current in onExit.
Partial allocation failure releases both and leaves boardReady false.
Its 2048-byte buffer payload matches the declaration, excluding allocator
and shared UI overhead. The prior C3 object measurement is 252 bytes.
No full runtime peak or fragmentation improvement is claimed.

OTP holds a fixed 100-byte page. Firmware initializes entropy then CTR-DRBG,
seeds, logs failure, and on exit zeroizes pageData before freeing DRBG then
entropy. Contexts are in the activity object, with possible internal library
allocations; the declared 256-byte additional heap estimate is not proven
by this simulator run. Prior C3 object size is 832 bytes; Clock's is 312.
These earlier compiler measurements were not repeated here.

SIMULATOR seedRng returns false and never initializes crypto contexts.
generatePage keeps the error state and clears bytes, so the button script
does not exercise real generation, reseeding, failure recovery or hardware
zeroization. Page navigation generates fresh data on hardware rather than
retrieving a saved page; page numbers are not persistent codebook identities.
No entropy/security claim follows from a successful fallback-screen test.

## Published artifacts and integration

As in batches 1–3, verification uses the inherited uncommitted firmware
snapshot. Batch 7 owns coherent implementation integration. This commit
publishes this evidence and BATCH_4_BASIC_APPS_SMOKE.patch containing the
smoke additions, new simulator rule tests and conditional friend declaration.
The patch applies to the batch 3 local smoke source and current local Game
of Life header; it is already applied in the working files. It requires the
pending app registrations and earlier smoke additions when integrated.
No unrelated production changes are included in this verification commit.

Tested SHA-256:

- Binary: f4372db40255b6085630fd0f25dcb744c97c941694c4ac86e3e276b1dfeff935
- Smoke cpp: 2e518efd5ec20a61e48bd2ecc49c703692913f0969672e1cd3a5946126964b14
- GameOfLifeActivity.h: b7ec144bbe8eaed795b3a941631fdb598a9a63a0b5fc8adfd5f486cb58c35fe1
- SimulatorGameOfLifeTest.cpp: 67e08fd7541c46010fe09a1727d6dc2e071c26987613213991965cc19829d14a
- SimulatorGameOfLifeTest.h: 98bd86728141595f172c91427fa6902e1bb1d6e6504e82570ab3919df3c0cabb

## Remaining hardware checks

On X4, Home → Tools → Tools category: verify Clock’s unavailable-RTC fallback.
On a supported RTC-equipped device, also verify advancing time, synced/pending
dates, date separators and UTC quarter-hour offsets. Verify OTP's real pad generation, errors, regeneration
and cleanup over repeated entries, without treating test pads as secrets.

Home → Tools → Games → Game of Life: confirm one generation per button tap,
restart resets Gen, displayed Alive matches the board, board fits the screen,
and Back returns Home. Measure free heap, largest block and task-stack
high-water marks for all apps before/during/after the relevant paths. Keep
C3 OOM and OTP crypto-resource checks open. No EPUB cache reset is needed.
