# Batch 6 — flash bookkeeping, 2026-09-09 UTC

The existing full-image gate is implemented and enabled. No new app wave,
firmware change, partition activation or flash operation occurred here.

## Current measured baseline

Rechecked the batch 5 working-snapshot artifact at
`/tmp/x4-batch5-firmware/.pio/build/default/firmware.bin`:

| Quantity | Bytes |
|---|---:|
| Actual image size | 6,316,256 |
| Smallest shipping app partition (app0/app1) | 6,553,600 |
| Remaining | 237,344 |
| Configured reserve | 262,144 |
| Reserve shortfall | 24,800 |

Image SHA-256: `ea4dba096cb7b40e7cf16cba8b9d80426c87b0946b6a9c3c538ce157c7ecfd85`.
Shipping partitions.csv SHA-256:
`8eab8ddac3fe3b14ab1f5b3a108b8982ac7a6851d7e3e28d1848d37f9a62d2ed`.
The image includes inherited local app work; it is not the size of a clean
published commit. No fresh firmware build was needed for documentation/tests.
The last C3 build passed with the low-headroom warning, as batch 5 recorded.

## Gate verification

`platformio.ini:102` already runs scripts/check_firmware_size.py. It reads
board_build.partitions, finds the smallest app slot, checks the actual .bin
length, fails on overflow and warns below custom_flash_reserve_bytes.
CROSSINK_FLASH_FAIL_UNDER_RESERVE=1 changes the reserve warning to failure.
The RAM manifest remains a separate declared-budget check.

Added `tools/port/test_firmware_size.py`, runnable with Python's standard
library only. One unittest with five subcases PASS: fitting image, low-reserve
warning, strict low-reserve failure, one-byte overflow, and exact slot size.
The fixture has unequal 1 MiB/2 MiB app slots and a small non-app partition,
so a gate choosing the larger slot or counting the data partition would fail.
Temporary sparse image files are deleted automatically. No production checker
logic or CI policy was changed.

## Planning correction

The ledger and manifest no longer claim a partition-aware gate is missing.
Historical launcher-wave and object-size numbers are labelled as context,
not extrapolated into a guaranteed app count. The uncommitted partition
proposal notes also remove the unsupported "400+ apps" capacity extrapolation;
that proposal still requires its physical stock-recovery gate.

No per-app flash_bytes field was added. Such numbers would be misleading
without controlled builds: shared libraries/assets, linker elimination,
alignment and configuration make object-file sums and app averages unreliable.
The documented method is before/after complete-image builds with identical
compiler/dependencies/target/flags/partitions, varying only the reviewed app
integration, and recording hashes, sizes and shared-dependency costs. Deltas
are configuration-specific and are not additive. The full-image gate remains
the release-size check. Further app expansion still requires its documented
gates; this bookkeeping does not grant partition or scope approval.

## Hardware follow-up

No cache reset or device check is needed to verify these documentation/test
changes. Before future partition activation, complete the existing physical
baseline/update/stock-recovery procedure in PARTITION_DECISION.md. Retain all
unverified hardware gates and leave shipping partitions unchanged meanwhile.
