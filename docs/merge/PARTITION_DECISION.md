# The flash partition question — decision and hardware gate

This document preserves a **proposal**, not an approved or active change.
The current firmware uses shipping `partitions.csv`. The alternative is at
`tools/port/partitions_repartitioned_PROPOSED.csv`; do not use it for the test
binary delivered on 2026-09-09. Its recovery procedure below remains unverified.

Earlier analysis proposed reclaiming the SPIFFS partition because app data
uses SD storage. That analysis does not establish safety of partition migration
or recovery, nor prove that the remaining app scope fits. Runtime consumers,
bootloader/update compatibility and real recovery must be checked before a
future activation decision. No device was flashed during this work.

## The proposed table

Reclaims all of `spiffs` (3,538,944 bytes) and gives it to `app0`/`app1`
evenly. `nvs`, `otadata`, and `coredump` are untouched — only the two app
slots grow, and the listed partitions run contiguously from 0x9000 to the 16 MB boundary
(the bootloader/table region below 0x9000 is reserved) (`app1` now ends at `0xFF0000`, precisely where `coredump` already
starts):

| Name | Type | SubType | Offset | Size | Size (decimal) |
|---|---|---|---|---|---|
| nvs | data | nvs | `0x9000` | `0x5000` | 20,480 |
| otadata | data | ota | `0xe000` | `0x2000` | 8,192 |
| app0 | app | ota_0 | `0x10000` | `0x7F0000` | 8,323,072 |
| app1 | app | ota_1 | `0x800000` | `0x7F0000` | 8,323,072 |
| coredump | data | coredump | `0xFF0000` | `0x10000` | 65,536 |

Each proposed OTA slot grows from 6,553,600 to 8,323,072 bytes (+1,769,472).
That is arithmetic for an unactivated proposal, not proof that the remaining
scope fits. Do not extrapolate an app count from the first five apps' object
sizes: linked dependencies, large assets and build options change the cost.
Use the actual-image gate and controlled before/after estimates described in
PORT_LEDGER.md; physical partition/recovery validation below remains mandatory.

## Why the shipping table remains unchanged

The physical stock-recovery/OTA gate has not been completed. This repository
contains only the proposed CSV for review; platformio.ini still selects the
original partitions.csv. Publishing this proposal does not activate it.

## The hardware gate — what the next session (with a device) must do

1. Flash a device with the **current, shipping** partition table and a known-good
   firmware build. Confirm it boots normally.
2. Build firmware against the **proposed** table above (point
   `board_build.partitions` at `partitions_repartitioned_PROPOSED.csv`,
   temporarily, in a branch — do not touch the live `partitions.csv` until
   after this test passes) and flash it via a **fresh serial flash**, not an
   OTA (changing the partition table itself is not an OTA-safe operation;
   it requires re-flashing the whole device, which is expected and fine for
   this test).
3. Confirm the repartitioned firmware boots, the app that was already
   installed still opens books/reads settings correctly (nvs/otadata were
   untouched, so this should be transparent, but "should be" is exactly what
   needs a device to confirm), and an OTA update **to another repartitioned
   build** succeeds and boots.
4. Confirm the existing stock recovery path (SD-card firmware update /
   `update.bin`, per `SdFirmwareUpdateActivity`) still works against a device
   running the new table — this is the specific rollback path rule 23 exists
   to protect, and it is the one most likely to have an unstated assumption
   about partition layout somewhere in the recovery flow.
5. Only after all four pass: replace `partitions.csv` with the proposed
   table's contents in the same change that documents the hardware test that
   passed (which device, which firmware versions, what was observed), and
   delete `partitions_repartitioned_PROPOSED.csv`.

## If repartitioning turns out to be blocked

Options 2 (build-time-optional tiles via a `-D` per tile) and 3 (cut scope)
from `PORT_LEDGER.md` remain the fallbacks, in that order, exactly as the
ledger already laid out. Nothing in this session's findings argues against
that ordering — it only found no reason *not* to attempt option 1 first, and
did the analysis that makes attempting it cheap and low-risk once a device is
available.
