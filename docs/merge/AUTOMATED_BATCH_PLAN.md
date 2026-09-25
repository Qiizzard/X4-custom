# Scheduled X4 merge batches

Requested 2026-09-05: start one bounded batch every five hours, targeting
roughly half the account's five-hour Codex allowance. This is a best-effort
usage target, not a guaranteed quota reservation or 2.5 hours of runtime.
Scheduled work uses the five-hour cadence; explicit manual continuation is allowed.

## User direction — feature ports first (2026-09-09 UTC)

This section supersedes the earlier verification-first queue and per-app
verification cadence. Port the approved Biscuit features in larger coherent
batches; run the comprehensive tests together after the implementation waves.
Do not spend another scheduled run reconciling old evidence before porting.
The historical queue/log below is retained only as history.

- Each eligible run ports several related features where feasible, with one
  compile sanity check for the batch. Defer host suites, simulator interaction
  scripts, soaks, performance measurements and exhaustive reviews to the final
  validation phase. Do not claim any deferred test passed.
- Commit and push each porting batch to the existing origin branch; keep all
  new ports marked wip/unverified until final validation. Record only a compact
  list of implemented features, limitations and next targets.
- Physical partition/recovery and SecureStore hardware gates apply to release
  validation, not a blanket halt on source implementation. Keep shipping
  partitions unchanged; do not flash or activate a proposed table. Respect C3
  memory limits, HAL boundaries, translations and real security semantics while
  porting. Do not copy a second full framebuffer or fake missing capabilities.
- Preserve the approved PORT_LEDGER scope; skip genuinely unresolved product
  choices and unsupported hardware without stalling other features. No new
  offensive/capture scope is authorized by this change of testing order.
- Preserve the five-hour cadence. Never buy credits or redeem resets, and do
  not overlap active manual work.

## User direction — full available usage (2026-09-23 local)

Use all available included account usage to accelerate the approved ports.
This supersedes all earlier percentage budgets, starting/weekly deferral
thresholds, reset-stop rules, unavailable-usage subtask limits and the
one-batch-per-wakeup restriction, including historical instructions below.

- Continue through larger coherent batches in the earliest actionable wave
  while actual account capacity and actionable work remain. Do not restart P1.
- Check usage at start and major boundaries for visibility, not reservation.
  A window reset or missing usage report is not a reason to stop.
- Checkpoint frequently so actual rate limits or interruptions do not lose
  work; resume at the next wakeup. Compile once per related porting batch,
  commit and push completed batches, then continue when capacity permits.
- Keep comprehensive testing in V1 and mark ports wip/unverified. Existing
  C3, flash-size, scope, security, hardware/recovery and no-purchase/no-reset
  constraints remain in force. Keep progress notes short.

## Active implementation queue

| Wave | Status | Work |
|---|---|---|
| P1 | implementation complete; validation deferred | Snake, Minesweeper, Tetris, Sudoku and Maze integrated (wip). Other approved games remain in P7. |
| P2 | implementation complete; validation deferred | Etch-A-Sketch, file-browser registration, Barcode and Key Bitting Charts integrated. |
| P3 | actionable implementation complete; validation deferred | Event Logger, Flashcards and Habit Tracker integrated (wip). Breadcrumb Trail/Vehicle Finder need product/location choices; Transit Alert awaits network infrastructure. |
| P4 | actionable implementation complete; validation deferred | Password Manager, Authenticator/TOTP QR and Stego Notes integrated (wip). Medical Card still needs its access policy. Hardware crypto/recovery gates remain unverified. |
| P5 | source implementation complete; validation deferred | Eight migration sites and Settings/KOReader-auth/OPDS/Calibre parents integrated (wip). Every picker caller passes a station token; legacy fallback removed. OTA/recovery and radio lifecycle hardware gates remain open. |
| P6 | actionable source implementation complete; validation deferred | Tools WiFi Networks entry integrated (wip), reusing the existing picker with its own bounded-lifetime radio hold. WiFi Scanner snapshot/detail and channel-count view integrated (wip); CSV export integrated; passive signal history integrated; DNS Lookup and bounded mDNS Browser with all-services/IPv6/CSV integrated (wip); Ping (TCP) and Host Scanner integrated (wip); HTTP Client integrated (wip); Packet Monitor/Probe Sniffer/Deauth Detector bounded views/exports integrated (wip); AP History/Wardriving/Network Change and Signal Locator/WiFi Heat Map/Perimeter Watch integrated (wip); Crowd Density history/chart and Device Fingerprint metadata integrated (wip). Vendor Lookup needs a dataset/flash decision; Full Sweep needs passive composite scope and BLE availability. Runtime validation deferred. |
| P7 | in progress | Matrix Rain, Voronoi, simplified Chess, Screen Decoy and Task Manager diagnostics integrated (wip); Network Monitor passive views, aggregation/rate history and event CSV integrated (wip). Remaining approved defense, comms, games and settings features; skip unresolved BLE/hardware/product choices and record them briefly. |
| V1 | deferred until ports finish | One consolidated host/simulator/soak/static-analysis/flash-budget and integration pass; fix failures together. |
| V2 | deferred until V1 | Produce test firmware and a concise hardware checklist; complete available device checks and record outstanding product/recovery gates. |

Start with the real source in ../biscuit-reference. P7 approved remaining ports are next; skip the documented P6 dataset/composite/BLE gates.
Porting is separate from the deferred V1 verification phase.

## Historical execution rules (superseded where conflicting above)


- Read AGENTS.md, PORT_LEDGER.md and SESSION_REPORT_2026-09-05.md, then inspect
  git status. Preserve the inherited uncommitted work. Do not redo verified
  work unless changes or failed checks justify it.
- At each wakeup, handle only the earliest incomplete actionable batch below.
  If it cannot finish within the usage budget, checkpoint and resume it at
  the next wakeup. Never overlap another run or an active manual edit session.
- Read account usage at start and at major boundaries. Target no more than
  40 percentage points of the five-hour window for implementation, leaving
  about 10 more for verification and checkpointing. Stop by an observed
  50-point increase or 85% total window usage, whichever comes first. If
  start usage is above 40%, defer this wakeup. If weekly usage is at least
  90%, defer. Shared account activity and delayed usage reporting mean this
  is approximate; do not promise an exact cap. If a window resets mid-run,
  checkpoint instead of treating the reset as a fresh budget for that run.
- If usage is unavailable, do one small subtask only and checkpoint within
  20 minutes of active work. Avoid repeated unchanged polling.
- User update, 2026-09-06: commit and push every completed batch to
  `origin/feat/x4-merge-v2-foundations-and-launcher` (Qiizzard/X4-custom)
  after verification and scoped review. Never force-push or bypass hooks.
  Do not include .agents, firmware-builds, generated files, local overrides,
  or unrelated changes. Flashing, live partition changes, purchases and
  usage-credit redemption remain prohibited. Report failed pushes and
  retain the local commit for retry.
- Keep hardware and product gates open. Do not fabricate a soak PASS,
  measured peak, static-analysis result, or done status. Static analysis is
  deferred to CI on this Mac.
- Record results below and in the ledger/port notes as appropriate. Notify
  only on completed work, meaningful failure, or newly required user action;
  remain quiet on unchanged/deferred wakeups.

## Queue

| Batch | Status | Bounded scope and exit criteria |
|---|---|---|
| 1 | complete | Finish the corrected lifecycle soak: build current simulator once, run scripts/run_simulator_soak_test.py for five apps with 50 cycles and 600000-ms holds in isolated filesystems. Verify explicit PASS markers and cleanup; record actual coverage. The last runs were manually stopped, never passed. Fix only a blocking harness defect; checkpoint if a larger defect appears. |
| 2 | complete | QR verification: retain passing entry/output/edit-cancel/exit smoke; add initial-cancel coverage and review the keyboard/QR transient budget. Measure what the simulator/compiler can establish, distinguish C3 estimates, and leave phone-scan/physical checks open. |
| 3 | complete | Cipher verification: script key/input/result/cancel paths through the existing simulator input harness, retain host transform tests, review keyboard lifetime/budget. Fix only findings in this app and leave genuinely unverified hardware behavior open. |
| 4 | complete | Review the remaining Clock/OTP/Game of Life evidence and resource lifetimes. Complete small host/simulator checks that require no device manipulation; record RTC, entropy and physical control checks precisely. Do not fake populated RTC or real OTP randomness to close a gate. |
| 5 | complete | Review the inherited compound-CSS fix, version-16 invalidation, fixture and cold/cache assertions. Add a focused missing regression only if needed, validate relevant builds and update cache/hardware instructions. No engine rewrite or runtime-font expansion. |
| 6 | complete | Close off-device flash-budget bookkeeping: reconcile actual full-image gate with stale documentation; add tested per-app flash attribution/planning support only if it is useful and clearly distinguishes estimates from linked image size. Keep shipping partitions unchanged. |
| 7 | complete | Review and locally commit verified work in coherent units (apps, CSS, navigation/SSID fixes, harness/docs as dependencies allow). Check formatting and relevant existing test/build evidence; rerun only checks invalidated by changes. Unverified implementations remain wip. Push the completed scoped commits. |
| 8 | pending | Reconcile ledger, CHANGELOG, port notes and session report; record commit IDs, outstanding working-tree items, hardware checklist and decision list. Identify whether any further work is actually unblocked. If only hardware/product gates remain, pause this automation and notify once. |

These batches have different real costs. A difficult batch may span several
wakeups; a small batch may use substantially less than half an allowance.
Do not spend extra quota merely to hit the target.

## Conditional continuation after gates clear

Do not schedule speculative app ports over the current flash/recovery gate.
After the user supplies the required hardware results/product choices,
append bounded batches using PORT_LEDGER's order: one security app per batch
(Authenticator, TOTP QR, Password Manager, Medical Card, Stego Notes), one
radio migration site per batch in RADIO_MIGRATION order, then one remaining
app per batch in ledger order. Each may require multiple wakeups and stays
wip until its entire checklist clears. All 60 todo rows remain real work;
this schedule does not declare them completed or permanently blocked.

## Run log

- 2026-09-05: Queue created after user stopped the interactive run. No batch
  completed by scheduling. Five-hour usage was 96%, weekly usage 15%.

- 2026-09-06 batch 1 checkpoint: simulator build PASS (4.07s). Started all
  five corrected 50-cycle/600000-ms soaks. All completed 50 cycles with zero
  measured Home-baseline delta; last observation had reached roughly 481s
  of idle hold. No final PASS verified yet. Logs and binary/source hashes:
  `/tmp/x4-soak-20260906-batch1/`; build log:
  `/tmp/x4-batch1-build-20260906.log`. Runner session 27791 was left running
  to finish its bounded 850-second timeout without further model work.
  **Next wakeup: inspect these logs for final explicit PASS markers and
  cleanup before rebuilding or restarting; complete batch 1 if proven.**
  Account usage rose from 20% to 48% to 100% (weekly 35% to 48%) despite
  mostly waiting. Attribution is unavailable; the approximate half-window
  budget could not be maintained. Stopped immediately on observing 100%.
  No code edits, commits, pushes or hardware actions in this batch.

- 2026-09-06 21:53 UTC: Batch 1 completed by verifying all five final PASS
  markers, holds >=600000 ms, zero post-cycle Home delta, idle growth
  160–352 bytes, and final cleanup within tolerance. Evidence retained in
  `verification/BATCH_1_SOAK_2026-09-06.md`. Next wakeup starts batch 2.
  No firmware edits, pushes, flashing or new app-done claims.

- 2026-09-06: User authorized pushing every completed batch. Batch 1
  publishes this queue and its verified evidence; the inherited uncommitted
  firmware implementation remains local pending scoped integration/review.

- 2026-09-08 UTC: Batch 2 complete: simulator build and full smoke PASS,
  including initial QR cancellation. Source-level QR heap/stack accounting
  reviewed; keyboard peak and C3 runtime measurements remain open. Evidence
  and QR-only smoke patch are in verification/BATCH_2_QR_2026-09-08.md and
  verification/BATCH_2_QR_SMOKE.patch. Firmware integration remains batch 7;
  QR stays wip. Next wakeup starts batch 3.

- 2026-09-08 09:00 UTC wakeup: Batch 3 complete. Final simulator build and
  eight-entry Cipher button script PASS; 14 Cipher host tests PASS. Resource
  lifetimes reviewed; peak keyboard heap remains unmeasured. Evidence and
  saved smoke patch: verification/BATCH_3_CIPHER_2026-09-08.md and
  verification/BATCH_3_CIPHER_SMOKE.patch. Runner timeout raised to 90s.
  Cipher remains wip; firmware integration remains batch 7. Next: batch 4.

- 2026-09-08 14:05 UTC wakeup: Batch 4 complete. Simulator build and full
  smoke PASS, including actual Game of Life block/wrapped-blinker/restart
  assertions and Clock/OTP/Game of Life button paths. Resource lifetimes
  reviewed. RTC, entropy, physical display and C3 runtime-memory gates stay
  open; all three apps remain wip. Evidence/test patch published under
  verification/BATCH_4_BASIC_APPS*. Firmware integration remains batch 7.
  Next wakeup: batch 5.

- 2026-09-08 19:05 UTC wakeup: Batch 5 complete. A new regression exposed
  bare compounds overriding tag-qualified compounds across class pairs;
  two-pass resolution fixes it. Cold/cache assertions and version-16
  rejection PASS; cache version is now 17. Simulator smoke and C3 default
  builds PASS (6,316,256-byte image; 237,344 bytes OTA headroom). Scoped CSS
  implementation/tests/fixture source are committed directly in this batch;
  app integration remains batch 7. Physical rendering checks remain open.
  See verification/BATCH_5_CSS_2026-09-08.md. Next wakeup: batch 6.

- 2026-09-09 00:06 UTC wakeup: Batch 6 complete. Rechecked 6,316,256-byte
  image / 6,553,600-byte smallest app slot (237,344 free; 24,800 below reserve).
  Corrected stale missing-gate prose and misleading app-count extrapolation;
  documented controlled image-delta planning without invented per-app budgets.
  Existing flash gate passes five new boundary subcases. No firmware or
  partition changes. See verification/BATCH_6_FLASH_2026-09-09.md.
  Next wakeup: batch 7, scoped integration of the remaining verified work.

- 2026-09-09 05:07 UTC wakeup: Batch 7 complete. Integrated app ports
  (99ca9764), harness/tests (3c70ae7a), and Home/SSID/comment fixes (8994be8e).
  Review caught and fixed Caesar signed overflow and partial malformed-decode
  output, with failing-then-passing regressions. 246 host tests, 16 UBSan
  Cipher tests, full simulator smoke, C3 build and budget checks PASS.
  Image 6,316,352 bytes; 237,248 OTA bytes free. All apps remain wip for
  documented gates. See verification/BATCH_7_INTEGRATION_2026-09-09.md.
  Next wakeup: batch 8 documentation reconciliation and remaining-gate audit.

- 2026-09-09 manual delivery: user requested GitHub synchronization and a test
  binary. Remaining relevant documentation reconciled and pushed; X3/X4
  updated.bin delivered from batch 7, checksum/hash validation PASS. Shipping
  partitions unchanged. Batch 8 final remaining-actionable-work audit stays
  pending; this delivery does not certify hardware gates or complete scope.

- 2026-09-09 user changed priority: feature-porting waves now precede combined
  testing. Remaining old batch 8 documentation audit is superseded. Current
  window usage was 93%, so no new port started during this workflow update.

- 2026-09-09 10:08 UTC wakeup, P1 chunk: ported Snake, Minesweeper and
  Tetris into Games with bounded storage and translated labels; all remain
  wip/unverified. Final C3 default compile sanity PASS (27.493s) after fixing
  a translation-format rejection and completing layout adjustments. Log:
  `/tmp/x4-p1-games-build-final.log`. Image 6,328,768 bytes, stock OTA free
  space 224,832 bytes (reserve warning remains). No host/simulator/soak or
  hardware tests run. Usage observed 4% → 31%, weekly 55% → 59%; no reset.
  Next wakeup resumes P1 with Sudoku and Maze; comprehensive testing stays V1.

- 2026-09-09 15:09 UTC wakeup, P1 final chunk: ported Sudoku and Maze
  into Games; both wip/unverified. Sudoku generation/solver are bounded and
  nonrecursive; Maze shares generation/BFS/path storage. C3 default compile
  sanity PASS (81.510s) after renaming a type that collided with Storage macro.
  Log: `/tmp/x4-p1b-build-final.log`. Image 6,338,624 bytes; unchanged OTA slot
  has 214,976 bytes free (reserve warning). No host/simulator/soak/hardware
  tests run. Usage observed 9% → 29%, weekly 61% → 64%, no reset. P1 source
  implementation complete; V1 validation remains deferred. Next: P2 offline
  creative tools. Other approved games remain scheduled in P7.

- 2026-09-09 20:09 UTC wakeup, P2 chunk: integrated Etch-A-Sketch with a
  4800-byte drawing and checked BMP export, and registered the existing file
  browser in Tools. Drawing is wip; new browser launcher route unverified.
  C3 default compile sanity PASS (135.556s), `/tmp/x4-p2a-build.log`. Image
  6,342,288 bytes, stock OTA headroom 211,312 bytes (reserve warning remains).
  No host/simulator/soak/hardware tests run. Usage observed 11% → 29%, weekly
  67% → 70%; no reset. Next wakeup continues P2: Barcode and Key Copier charts.

- 2026-09-10 P2 final chunk: integrated Barcode (Code 128B, Code 39,
  EAN-13) and reference-only Key Bitting Charts; both wip/unverified. Barcode
  input/label buffers bounded, unsupported/over-wide payloads rejected, and
  mismatched Code 128 symbol widths corrected against the linked reference.
  C3 compile sanity PASS (202.452s), `/tmp/x4-p2b-build.log`. Image 6,351,808
  bytes, unchanged OTA headroom 201,792 bytes; reserve warning remains.
  No host/decoder/simulator/soak/hardware tests run. Usage observed 0% → 33%,
  weekly 78% → 83%, no reset. P2 source implementation complete. Next: P3
  Event Logger, Flashcards and Habit Tracker. Full validation remains V1.

- 2026-09-10 P3 checkpoint: Event Logger integrated, wip/unverified. Bounded
  50-note ring and 64 KiB log, explicit-submit writes, truthful uptime labels.
  C3 compile sanity PASS (150.938s), `/tmp/x4-p3a-build.log`; image 6,356,528
  bytes, unchanged OTA headroom 197,072 bytes (reserve warning remains). No
  host/simulator/soak/hardware tests run. Usage observed 40% → 70%, weekly
  84% → 89%; no reset. Stopping this chunk; next eligible run continues P3
  with Flashcards and Habit Tracker. Existing local unrelated files preserved.

- 2026-09-12 11:01 UTC wakeup, P3 checkpoint: Flashcards integrated (wip),
  bounded to 16 deck names and 32 cards with session grading. C3 compile
  sanity PASS (143.760s), `/tmp/x4-p3b-build.log`. Image 6,361,984 bytes;
  stock OTA headroom 191,616 bytes, reserve warning remains. No host/parser/
  simulator/soak/hardware tests run. Usage observed 17% → 35%, weekly 3% → 6%;
  no reset. Next eligible run: Habit Tracker. Existing unrelated files preserved.

- 2026-09-12 16:02 UTC wakeup, P3 checkpoint: Habit Tracker integrated
  (wip), explicit sessions, bounded habits, debounced alternating save slots.
  Final C3 compile PASS (128.802s), `/tmp/x4-p3c-build-final.log`; reran after
  a final translation key missed initial generation. Image 6,367,552 bytes;
  stock OTA free 186,048 bytes, reserve warning remains. No host/simulator/
  soak/hardware tests run. Usage 0% → 47%, weekly 6% → 14%, no reset.
  P3 actionable source work complete; unresolved location/network apps stay
  gated. Next: P4 SecureStore consumers. Full validation remains V1.

- 2026-09-12 21:03 UTC wakeup, P4 checkpoint: added bounded encrypted SD
  file adapter using SecureStore and HalStorage, no registered vault UI yet.
  Exclusive create preserves existing paths; failed valid-buffer loads wipe
  plaintext and scratch. Consumer records, secure keyboard lifetime, editing
  and lock/exit behavior remain next. C3 compile PASS (123.344s),
  `/tmp/x4-p4a-build.log`; adapter source compiled, unused adapter discarded
  at link (image unchanged: 6,367,552 bytes, headroom 186,048). No runtime
  crypto/file, host, simulator, soak or hardware tests run. Usage observed
  20% → 41%, weekly 18% → 21%, no reset. P4 remains in progress.

- 2026-09-13 02:04 UTC wakeup, P4 checkpoint: fixed eight-record password
  model/codec and fixed-buffer secret-entry child activity implemented. No
  Password Manager screen registered yet. Next: create/unlock/edit/save and
  lock/exit integration with encrypted file adapter. C3 compile PASS
  (165.926s), `/tmp/x4-p4b-build.log`; image 6,367,872 bytes, unchanged OTA
  headroom 185,728 bytes (reserve warning). New source files compiled;
  unused components are not evidence of a working vault. No host/runtime
  crypto/input/simulator/soak/hardware tests run. Usage 20% → 42%, weekly
  25% → 29%, no reset. P4 remains in progress.

- 2026-09-13 07:04 UTC wakeup, P4 checkpoint: Password Manager integrated
  in Tools (wip): create/repeat key, unlock, add/replace/delete, encrypted
  previous-file retention, timed reveal and idle/cancel/exit wiping. One C3
  compile sanity PASS (152.050s), `/tmp/x4-p4c-build.log`; image 6,378,976
  bytes, stock OTA free 174,624 bytes (reserve warning). No host, runtime
  crypto, simulator, soak or hardware tests run. Usage observed 1% → 18%,
  weekly 34% → 36%, no reset. Next eligible run: Authenticator/TOTP QR.
  Recovery UI and hardware crypto gates remain open; unrelated files preserved.

- 2026-09-13 P4 checkpoint: Authenticator and TOTP QR integrated (wip),
  shared encrypted eight-account vault, strict Base32, fixed SHA-1/6/30 codes,
  code-only QR, per-boot synced-time gate and RTC-less Settings sync path.
  One C3 compile sanity PASS (209.579s), `/tmp/x4-p4d-build.log`; image
  6,382,192 bytes, stock OTA free 171,408 bytes (reserve warning). No host,
  runtime crypto, simulator, soak or hardware tests run. Usage observed
  0% → 28%, weekly 38% → 42%, no reset. Next eligible run: Stego Notes;
  Medical Card still needs access policy. Existing unrelated files preserved.

- 2026-09-13 18:28 UTC wakeup, P4 checkpoint: encrypted BMP trailer hide/reveal
  helper implemented, bounded notes/carriers, exclusive new output, original
  preserved. Stego Notes UI is still next; no new app exposed. Final C3 compile
  PASS (34.504s), `/tmp/x4-p4e-build-final.log`; recompiled after correcting
  file-size checks to the 64-bit HAL API. Unused helper is discarded at link;
  image remains 6,382,192 bytes, OTA free 171,408 bytes. No host/runtime crypto/
  simulator/soak/hardware tests run. Usage observed 39% → 54%, weekly 44% → 46%,
  no reset. Unrelated files preserved; hardware/product gates remain open.

- 2026-09-13 23:29 UTC wakeup, P4 checkpoint: Stego Notes registered in Tools
  (wip), bounded file selection, masked chunked note input, confirmed key,
  exclusive new BMP output and timed reveal/secret wiping. One C3 compile
  sanity PASS (135.161s), `/tmp/x4-p4f-build.log`; image 6,389,472 bytes,
  stock OTA free 164,128 bytes (reserve warning). No host/runtime crypto,
  simulator, soak or device tests run. Usage observed 2% → 19%, weekly
  48% → 51%, no reset. P4 actionable source ports complete; Medical Card
  access policy and hardware gates remain open. Next: P5 radio migration.

- 2026-09-14 04:31 UTC wakeup, P5 checkpoint: ClockSync and FontDownload
  acquire station ownership around their existing WiFi picker; checked owner
  release/status, busy handling and fallible picker allocation added. Font-change
  restart retained after owned teardown. Source integrated, runtime unverified.
  One C3 compile sanity PASS (145.447s), `/tmp/x4-p5a-build.log`; image
  6,391,744 bytes, stock OTA free 161,856 bytes (reserve warning). No host,
  simulator, network, soak or hardware tests run. Usage observed 15% → 41%,
  weekly 55% → 59%, no reset. Next: KOReaderSync; picker and remaining radio
  migrations still pending. OTA/recovery and unrelated files unchanged.

- 2026-09-14 09:32 UTC wakeup, P5 checkpoint: KOReaderSync station ownership
  integrated (wip), owner-checked early/exit release, station/IP preflight and
  fallible picker; existing reader reboot and TLS/EPUB flow retained. One C3
  compile sanity PASS (31.696s), `/tmp/x4-p5b-build.log`; image 6,392,080
  bytes, stock OTA free 161,520 bytes (reserve warning). No host, simulator,
  live sync/network, soak or hardware tests run. Usage observed 18% → 38%,
  weekly 63% → 66%, no reset. Next: CrossPointWebServer/AP ownership mode.
  Unrelated files, OTA/recovery code and shipping partitions preserved.

- 2026-09-16 19:30 UTC wakeup, P5 checkpoint: bounded owner-checked AP
  acquisition/address API and AP cleanup added to RadioManager. Web-server
  consumer wiring remains next; no site-4 completion or runtime AP claim.
  One C3 compile sanity PASS (220.334s), `/tmp/x4-p5c-build.log`; image
  6,392,272 bytes, stock OTA free 161,328 bytes (reserve warning). No host,
  simulator, radio, network, soak or hardware tests run. Usage observed
  21% → 51%, weekly 71% → 75%, no reset. Next: CrossPointWebServer AP/STA
  ownership and existing restart/service cleanup integration. Unrelated work,
  OTA/recovery code and shipping partitions preserved.

- 2026-09-17 00:30 UTC wakeup, P5 checkpoint: web-server AP/STA ownership,
  address/status/RSSI, picker-cancel release and guarded startup/error/exit
  cleanup integrated (wip). Fast-exit restart and legacy Calibre fallback remain.
  One C3 compile sanity PASS (28.283s), `/tmp/x4-p5d-build.log`; image
  6,393,760 bytes, stock OTA free 159,840 bytes (reserve warning). No live
  transfer/radio, host, simulator, soak or hardware tests run. Usage observed
  22% → 46%, weekly 81% → 84%, no reset. Next: the two ESP-NOW sync sites;
  shared picker/OTA migrations remain pending. Unrelated work preserved.

- 2026-09-19 15:13 UTC wakeup, P5 checkpoint: NearbyStatsSync and
  NearbyBookPositionSync acquire named ESP-NOW holds; manager configures channel
  and power save. Partial startup failures clean up, failed startup cannot send,
  and callbacks/ESP-NOW stop before owner-checked radio shutdown. Reader restart
  semantics and packet/security formats retained. Source wip/unverified.
  One C3 compile sanity PASS (28.639s), `/tmp/x4-p5e-build.log`; image
  6,394,528 bytes, stock OTA free 159,072 bytes (reserve warning). No host,
  simulator, two-device sync, soak or hardware tests run. Usage observed
  24% → 41%, weekly 4% → 6%, no reset through compile/checkpoint boundary.
  Next: shared WiFi picker; OTA/recovery release gates remain open.
  Unrelated work and shipping partitions preserved.

- 2026-09-19 20:14 UTC wakeup, P5 picker checkpoint: explicit parent tokens
  wired through four managed callers; denied/lost ownership prevents picker
  operations and teardown, cancellation preserves the parent's reservation,
  unfinished exits clean up. Wip; SDK migration/legacy handoff remains next.
  C3 compile PASS, final 25.824s (`/tmp/x4-p5f-final-build.log`), after
  initial 29.649s compile and a Done-button correction. Image 6,394,976 bytes,
  OTA free 158,624 bytes; reserve warning remains, partitions unchanged.
  No host/simulator/soak/device tests. Usage 6% → 17%, weekly 8% → 10%;
  no window rollover (reset timestamp varied by one second). Unrelated work
  preserved. Resume site 7 before OTA; five-hour cadence remains unchanged.

- 2026-09-20 01:15 UTC wakeup, P5 picker scan checkpoint: async scan start,
  polling, bounded result reads and cleanup moved into owner-checked manager
  APIs. Retains at most 40 SDK results plus manual SSID entry; SDK internal
  allocation remains outside that cap. Source wip; association/legacy handoff
  next. One C3 compile PASS (32.470s), `/tmp/x4-p5g-build.log`; image
  6,395,600 bytes, stock OTA free 158,000 bytes (reserve warning).
  No host/simulator/soak/device tests. Usage 8% → 17%, weekly 12% → 13%
  through compile/checkpoint boundary, no reset. Unrelated work preserved;
  partitions and OTA/recovery code unchanged. Five-hour cadence continues.

- 2026-09-20 06:15 UTC wakeup, P5 connection checkpoint: picker preparation,
  begin and disconnect paths moved behind manager authorization; preserves
  non-erasing disconnect, strongest-AP policy, parent holds and credentials.
  Hostname uses 46 bytes of local buffers, no new heap storage. Wip/unverified.
  C3 compile PASS (27.781s), `/tmp/x4-p5h-build.log`; image 6,396,144 bytes,
  stock OTA free 157,456 bytes, reserve warning unchanged. No deferred suites
  or device tests. Usage 10% → 15%, weekly 15% → 16% at compile boundary,
  no reset. Next: status/events and legacy handoff, then OTA. Unrelated work,
  shipping partitions and five-hour cadence preserved.

- 2026-09-20 13:14 UTC wakeup, P5 status/events checkpoint: picker uses
  manager status snapshots/MAC reads and centralized event logging; no direct
  WiFi/ESP SDK calls remain in that activity. Atomic callback-shared flags;
  legacy handoff remains next, source wip/unverified. C3 compile PASS
  (27.492s), `/tmp/x4-p5i-build.log`; image 6,396,768 bytes, stock OTA free
  156,832 bytes, reserve warning remains. No deferred suites/device tests.
  Usage 21% → 34%, weekly 17% → 19% through compile/checkpoint, no reset.
  Unrelated work, partitions and five-hour cadence preserved.

- 2026-09-20 18:15 UTC wakeup, P5 parent batch: Settings Wi-Fi and KOReader
  authentication/sign-up now acquire/pass/release named holds. Settings releases
  on picker return/global exit; auth preserves own-session restart and refuses
  foreign-session borrowing/restart. Fallible picker allocation, source wip.
  One C3 compile PASS (37.353s), `/tmp/x4-p5j-build.log`; image 6,397,296
  bytes, stock OTA free 156,304 bytes, reserve warning remains. No deferred
  suites/device tests. Usage 12% → 26%, weekly 22% → 24% at checkpoint,
  no reset. Next OPDS/Calibre, then OTA. Unrelated work/partitions preserved.

- 2026-09-20 23:15 UTC wakeup, P5 OPDS/Calibre batch: named parent holds,
  guarded requests/server startup, fallible picker/server allocation, preserved
  restart paths and removal of web parent's legacy Calibre teardown. Source
  wip/unverified. One C3 compile PASS (23.453s), `/tmp/x4-p5k-build.log`;
  image 6,397,440 bytes, stock OTA free 156,160 bytes, reserve warning remains.
  No deferred suites/device tests. Usage 14% → 28%, weekly 27% → 29%
  through checkpoint, no reset. Next OTA then remove picker legacy fallback.
  Unrelated work and shipping partitions preserved; five-hour cadence unchanged.

- 2026-09-21 04:16 UTC wakeup, P5 source completion: OTA owns its station,
  guards update check/install entry, uses fallible picker allocation and releases
  on exit. All nine picker callers require tokens; null-owner fallback removed.
  Updater validation/install/success reboot and partition table unchanged.
  C3 compile PASS (29.433s), `/tmp/x4-p5l-build.log`; image 6,397,552 bytes,
  stock OTA free 156,048 bytes, reserve warning remains. Source wip/unverified;
  no flashing, host/simulator/soak/device tests. Usage 15% → 27%, weekly
  32% → 34% through checkpoint, no reset. Next P6 approved ports; release
  hardware/recovery gates stay open. Unrelated work and five-hour cadence preserved.

- 2026-09-21 09:16 UTC wakeup, P6 WiFi Connect: Tools > WiFi Networks
  registers the existing picker with a standalone tools_wifi hold. Scan/select,
  hidden SSIDs and credential persistence are reused; success/cancel/global
  exit release the hold. Existing parent-owned constructor remains unchanged.
  Source wip/unverified. One C3 compile PASS (191.767s after dependency restore),
  `/tmp/x4-p6a-build.log`; image 6,397,824 bytes, OTA free 155,776 bytes,
  reserve warning remains. No host/simulator/soak/device tests. Usage observed
  17% → 48%, weekly 38% → 43%, no reset. Unrelated work preserved.
  Device check: Tools > WiFi Networks, connect/save/forget/cancel, global Home,
  then Clock Sync; verify tools_wifi releases and denied entry leaves another
  owner untouched. No cache reset. Next remaining approved P6 utilities/recon.

- 2026-09-21 14:17 UTC wakeup, P6 WiFi Scanner core: adapted Biscuit's
  snapshot/detail concept with 40 fixed AP records (1,680 bytes in the fallibly
  allocated activity), passive SDK scans, RSSI ordering, sanitized SSID display,
  BSSID/channel/security details and manual rescan. No association/export/live
  charts yet. SDK internal scan memory is not capped by the result buffer.
  Source wip/unverified; keyboard/button browsing, touch Back supported.
  Initial compile found a theme API mismatch; corrected final C3 compile PASS
  (69.013s), `/tmp/x4-p6b-final-build.log`. Image 6,402,160 bytes, stock OTA
  free 151,440 bytes, reserve warning remains. No host/simulator/soak/device tests.
  Usage reached 40% from 0%, weekly 44% → 50% at correction/build boundary;
  implementation stopped for verification/checkpointing. Reset timestamp moved
  one second without a window rollover. Unrelated work/partitions preserved.
  Device: Recon > WiFi Scanner, inspect APs with Left/Right, Confirm rescan,
  Back/global Home then Clock Sync; verify passive-only RF behavior, capped
  results, failed scan/denied owner, orientation and heap. No cache reset.
  Next: remaining approved scanner views/export and P6 utilities.

- 2026-09-21 19:19 UTC wakeup, P6 scanner channel view: 13-channel AP-count
  chart and selected-channel peak/mean RSSI derived from the existing capped
  passive snapshot. Page/Up/Down switches views; Left/Right selects channel.
  Counts are labeled observed snapshot data, not congestion recommendations.
  Uses 56 bytes of temporary arrays, no additional scans or heap buffers.
  One C3 compile PASS (127.178s), `/tmp/x4-p6c-build.log`; image 6,403,600
  bytes, stock OTA free 150,000 bytes, reserve warning remains. Source wip;
  no host/simulator/soak/device tests. Usage 20% → 42%, weekly 55% → 59%,
  no reset at checkpoint. Unrelated work/partitions preserved.
  Device: Recon > WiFi Scanner, toggle views, inspect empty/populated channels,
  rescan, rotate and exit to Clock Sync; check chart bounds/ownership. No cache
  reset. Next: approved signal history/CSV export and remaining P6 utilities.

- 2026-09-22 00:21 UTC wakeup, P6 scanner CSV export: Page Back saves up to
  40 retained rows to exclusive /crossink/wifi/scan-00.csv through scan-99.csv.
  Streams bounded rows, quotes/doubles SSID quotes and prefixes formula-like
  SSIDs for spreadsheet safety. Exports sanitized display SSIDs; no credentials.
  Checked mkdir/open/write/sync/close, best-effort partial-file removal, visible
  success/failure. No growing CSV string or added radio scan. Source wip.
  C3 compile PASS (160.48s), `/tmp/x4-p6d-build.log`; image 6,405,088 bytes,
  stock OTA free 148,512 bytes, reserve warning remains. No deferred suites or
  hardware tests. Usage 21% → 46%, weekly 63% → 67%, no reset at checkpoint.
  Device: export repeated snapshots, inspect commas/quotes/formula-like SSIDs,
  all 100 occupied slots, absent/full/removed SD, then exit and reuse radio.
  No cache reset. Next signal history, then remaining P6 utilities.
  Unrelated work and partitions preserved.

- 2026-09-22 05:23 UTC wakeup, P6 scanner signal history: view cycle now
  details/channels/signal. Pins selected BSSID, stores 40 outcomes in an 80-byte
  fixed ring, shows current/min/mean/max, and leaves gaps for unseen/failed
  scans. Passive full scans pause five seconds after completion while signal
  view is active; no sampling in other views or during export-result display.
  Missing means absent from the capped snapshot, not proof the AP is offline.
  Blocking scan behavior remains; charts show scan sequence, not fixed time.
  Source wip/unverified. C3 compile PASS (142.528s), `/tmp/x4-p6e-build.log`;
  image 6,406,784 bytes, stock OTA free 146,816 bytes, reserve warning remains.
  No host/simulator/soak/device tests. Usage 22% → 46%, weekly 72% → 75%,
  no reset at checkpoint. Unrelated work/partitions preserved.
  Device: select an AP, cycle to signal view, vary signal/remove AP, run past
  40 scans, force scan failure, switch views/export/exit and reuse radio.
  Check graph gaps, BSSID identity, bounds/orientation and heap; no cache reset.
  Next remaining approved P6 network utilities/recon ports.

- 2026-09-23 00:03 UTC wakeup, P6 DNS Lookup: Tools entry with named
  station hold, fallible picker/keyboard, 253-byte ASCII hostname limit and one
  IPv4/IPv6 result. Manager validates input/ownership and rechecks connection
  after synchronous SDK resolution. SDK timeout/cache policy retained; no
  background queries. Fixed activity buffers plus one bounded SDK formatting
  String per lookup; no recurring allocation loop. Source wip/unverified.
  C3 compile PASS (201.434s), `/tmp/x4-p6f-build.log`; image 6,410,544 bytes,
  stock OTA free 143,056 bytes, reserve warning remains. No host/simulator,
  soak or live network/device tests. Usage 19% → 48%, weekly 80% → 84%
  at late-build boundary, no reset. Unrelated work/partitions preserved.
  Device: Tools > DNS Lookup, connect, resolve known/missing/long/invalid names,
  try IPv4/IPv6 results, drop Wi-Fi and cancel/global Home; verify radio reuse.
  No cache reset. Next remaining approved P6 utilities/recon ports.

- 2026-09-23 05:04 UTC wakeup, P6 mDNS Browser core: Tools service selection
  (ten TCP service types), single 2-second SDK query, eight fixed records with
  name/hostname/IPv4/port, manual repeat and owned station cleanup. Refuses an
  existing mDNS responder; frees query results and its responder before returning.
  Result storage is 1,184 bytes in the fallibly allocated activity; SDK transient
  TXT/address/task memory still needs measurement. Source wip/unverified; names
  are display-sanitized/truncated. No all-services aggregation, CSV or IPv6 detail
  yet; these remain P6 work, alongside other approved utilities/recon.
  One C3 compile PASS (126.098s), `/tmp/x4-p6g-build.log`; image 6,415,104 bytes,
  unchanged OTA free 138,496 bytes, reserve warning remains. No host/simulator,
  live queries, soak or hardware tests. Usage 3% → 16%, weekly 87% → 89% at
  late-build boundary, no reset. Unrelated work and partitions preserved.
  Device: Tools > mDNS Browser, connect, select/discover a known TCP service,
  browse results with Left/Right; check empty, >8, IPv6-only and lost-Wi-Fi cases.
  Back returns to selection then exits; test global Home, busy responder/radio,
  repeated-query heap and subsequent Clock Sync/Calibre. No cache reset.

- 2026-09-24 UTC manual continuation, P6 mDNS IPv6/CSV: records retain first
  IPv4 and IPv6 addresses (numeric scope when supplied by SDK). Page Back exports
  up to eight records to exclusive `/crossink/mdns/services-00.csv` through
  `services-99.csv`; streams quoted fields with formula-prefix protection,
  checks write/sync/close and attempts partial-file removal on failure.
  Result storage grows 384 bytes to 1,568 bytes in the activity; export uses
  bounded stack fields, no document-sized buffer. All-services aggregation
  remains next; source is wip/unverified. One C3 compile PASS (225.112s),
  `/tmp/x4-p6h-build.log`; image 6,418,000 bytes, unchanged OTA free 135,600
  bytes, reserve warning remains. No host/simulator/soak/live network/device
  tests. Usage 1% → 15%, weekly 0% → 2% at mid-build boundary, no reset.
  Unrelated work and shipping partitions preserved.
  Device: discover IPv4-only/IPv6-only/dual-stack services, inspect address
  display and export columns; check commas/quotes/formula-like names, occupied
  export slots, absent/full/removed SD, dismiss export result and exit/reuse
  radio. Earlier exports must remain intact. No cache reset.

- 2026-09-23 local: user authorized all available included usage. Removed
  artificial usage deferrals and one-batch-per-wakeup cap from the active
  instructions and saved automation; five-hour cadence and safeguards retained.

- 2026-09-24 06:08 UTC run, P6 mDNS completion checkpoint: all-services
  selection steps through ten TCP types, one 2-second query per loop. Retains
  eight total records plus eight service-index bytes; CSV preserves each row's
  type. Back cancels between queries; failed/cancelled/capped sweeps are marked
  partial. Source wip/unverified. C3 compile PASS (144.829s),
  `/tmp/x4-p6i-build.log`; image 6,418,656 bytes, OTA free 134,944, reserve
  warning remains. No deferred suites/device tests. Usage 10% → 23%, weekly
  7% → 9%; continuing under full-usage policy. Device: browse All services,
  cancel midway, exceed cap, drop Wi-Fi, export mixed types and reuse radio.
  No cache reset. Next Ping/remaining P6 utilities; unrelated work preserved.

- 2026-09-24 06:08 UTC run, P6 TCP utilities batch: Ping (TCP) performs five
  port-80 attempts with connection timings, no ICMP/loss claim. Host Scanner
  discovers TCP-80 responders in the owned station's local /24 slice or smaller
  subnet, excludes own/network/broadcast addresses, caps at 32 eight-byte records,
  checks 14 common TCP ports on explicit selection and streams numbered CSV.
  Shared manager uses nonblocking sockets with bounded waits, explicit close,
  owner checks and subnet-change aborts. UI cancels between probes; no worker
  tasks or application receive buffers. SDK socket memory needs device checks.
  Source wip/unverified. C3 compile PASS (173.720s), `/tmp/x4-p6j-build.log`;
  image 6,428,800 bytes, OTA free 124,800, reserve warning persists. No deferred
  suites/live network/device tests. Usage 44% five-hour/13% weekly at boundary;
  continuing HTTP Client under full-usage policy. Unrelated files preserved.
  Device: Ping reachable/closed/unresolved targets, cancel and radio reuse;
  Host Scanner /24 and smaller subnets, DHCP changes, >32 responders, partial
  port checks, missing/full SD and CSV tested/open columns. Check heap and
  orientation. No cache reset; no flashing/partition changes.

- 2026-09-24 06:08 UTC run, P6 HTTP Client: explicit GET/plain-text POST menu,
  fixed 256-byte URL/512-byte body and 1024-byte response preview. Helper uses
  esp_crt_bundle_attach, refuses embedded credentials/encoded authorities, does
  not follow redirects, and always cleans the client. Preview flattens whitespace
  and replaces non-ASCII bytes; cap is visible. SDK read/connect timeouts apply;
  write/read loops also check elapsed time and ownership. DNS/TLS setup timing
  and SDK allocation peaks require hardware measurement; no hard overall deadline
  claimed. Fixed activity data ~1.8 KiB; large SDK config allocated fallibly off
  task stack. Source wip/unverified. C3 compile PASS (162.499s),
  `/tmp/x4-p6k-build.log`; image 6,434,560 bytes, OTA free 119,040, reserve warning.
  No deferred suites/live requests/device tests. Usage read unavailable at this
  boundary; continuing with checkpoints per updated policy. Unrelated work kept.
  Device: valid/invalid TLS, GET/POST, 204/redirect/error/chunked/large responses,
  timeout/disconnect, paging/orientation and repeated entry/exit heap. No cache
  reset. Next passive recon; shipping partitions unchanged.

- 2026-09-24 06:08 UTC run, P6 passive monitor batch: shared fixed 2 KiB
  callback queue, 96-byte frame prefix, eight recent events, channel/hop/pause
  controls. Packet Monitor counts types/drops and writes exclusive PCAP files
  capped at 1 MiB; timestamps are dequeue uptime, FCS stripped per native SDK
  length contract. Probe Sniffer displays source/SSID/RSSI; Deauth Detector
  counts deauth/disassociation frames and reports protected/short reasons as
  unavailable. No attack attribution. Callback only copies; bounded loop parses
  and writes SD. Manager owner checks plus short dispatch/detach critical section
  prevent context teardown racing an in-flight copy. Runtime/RF behavior unverified.
  Source wip; channel chart/CSV, probe aggregation/CSV and deauth spike view remain.
  Initial compile rejected category ordering; corrected final C3 PASS (110.438s),
  `/tmp/x4-p6l-final-build.log`. Image 6,443,728 bytes, OTA free 109,872, reserve
  warning persists. No deferred suites, live capture or device tests. Usage
  84% five-hour/19% weekly at correction boundary; durable checkpoint before
  more work. Unrelated work preserved; no flashing or partition changes.
  Device: each Recon entry, fixed/hopping channels, pause/resume/Home and next
  radio app; malformed/protected/short frames, queue pressure and heap. Packet
  Monitor: inspect PCAP prefix lengths/uptime, cap, all slots, SD removal/failure
  and partial-file status. No cache reset; RF passivity remains a hardware gate.

- 2026-09-24 11:09 UTC run, P6 monitor summaries: channel frame chart/CSV,
  24 fixed source-MAC rows (1,152 bytes), probe last-SSID/RSSI/count summaries
  and quoted/formula-safe CSV. Packet Up toggles chart, Down exports; Probe
  Page Back exports. Deauth burst threshold matches reference (five frames in
  approximately two seconds), labels actual processing interval, Page Back
  clears; no device-count or attack claim. Source wip/unverified. One C3
  compile PASS (152.545s), `/tmp/x4-p6m-build.log`; image 6,447,456 bytes,
  OTA free 106,144, reserve warning. No deferred suites/device/live capture tests.
  Usage 16% → 33%, weekly 24% → 27%; continuing snapshot/logger ports. Unrelated
  work/partitions preserved. Device: >24 source MACs, repeat/randomized MACs,
  mixed SSID quoting, saturated channel counters/unequal dwell, pause/export,
  queue drops and burst acknowledge. Check SD failures and radio reuse; no cache reset.

- 2026-09-24 11:09 UTC run, P6 AP journal batch: AP History repeats capped
  passive observations at 1/5/10/30-minute pauses; Wardriving logs first sightings
  at 10-second pauses. Both track at most 64 BSSIDs, log to new numbered files
  capped at 1 MiB, close after each scan, and retain partial logs on failure.
  Network Change compares at most 40 BSSIDs with new/absent/metadata labels,
  deduplicates results and persists explicit version-1 CRC-protected baselines
  in increasing exclusive slots. Invalid latest files fall back to earlier valid
  slots; failed saves preserve the active baseline. Fixed shared activity buffers
  ~5.4 KiB. Source wip/unverified. C3 PASS (140.892s), `/tmp/x4-p6n-build.log`;
  image 6,456,128 bytes, OTA free 97,472, reserve warning persists. No deferred
  suites/live scan/device tests. Continuing remaining P6; unrelated work preserved.
  Device: repeated/empty/capped scans, pause/resume, first/last uptime, duplicate
  BSSIDs, >64 identities, SD removal/full logs; baseline reload, corruption/short
  files, exhausted slots, channel/security/SSID changes and absent snapshot
  entries. Check radio reuse and heap. No cache reset or partition changes.

- 2026-09-24 11:09 UTC run, P6 signal/watch batch: Signal Locator uses 40
  fixed AP rows (1,680 bytes) and three bounded sample positions, reports
  missing readings and strongest observed RSSI without location inference.
  Heat Map reuses the AP journal buffers for RSSI CSV at five-second pauses,
  capped at 1 MiB/10,000 rows. Perimeter Watch reuses 64 records for a volatile
  baseline and new BSSID observations, with pause/reset and exclusive CSV export;
  no intrusion attribution. Source wip/unverified. C3 compile PASS (144.550s),
  `/tmp/x4-p6o-build.log`; image 6,462,240 bytes, OTA free 91,360, below the
  262,144-byte reserve. Deferred suites/device/RF tests not run. Usage now
  64% five-hour/31% weekly. Unrelated work and partitions preserved.
  Device: missing/reappearing target samples, cancel/retry, baseline reset,
  capped identities, pause/resume, journal row/file caps, SD failure and export
  quoting; check radio reuse and heap after repeated entry/exit. No cache reset.

- 2026-09-24 16:09 UTC run, P6 Crowd Density: reused fixed passive queue and
  24-MAC table; 60 fixed history rows add 720 bytes. Thirty-second processing
  windows show actual elapsed time and untracked frames, with cumulative queue
  drops visible. Resume discards the partial window; counts never estimate
  people/devices. Source wip; chart remains, runtime/RF tests deferred. C3
  compile PASS (130.300s), `/tmp/x4-p6p-build.log`; image 6,463,792 bytes,
  OTA free 89,808, reserve warning persists. Initial usage 4%/weekly 33%.
  Full Sweep reference requires BLE and makes unsupported hidden-SSID threat
  inferences; marked blocked pending passive-only composite scope. No other
  manual repo task active; unrelated work retained. No deferred suites run.
  Device: repeated/randomized/>24 source MACs, 60-window wrap, browsing,
  pause/resume, channel switching/hopping and queue pressure; check exit radio
  release/heap. No cache reset, flashing or partition changes.

- 2026-09-24 16:09 UTC run, P6 metadata/chart batch: Device Fingerprint
  reuses the 24-row probe summary/CSV and owner-checked capture; displays MAC
  administration bit, RSSI/channel/count/SSID and explicitly unknown OS.
  Reference probe-count OS guesses are not reliable identification. Crowd
  history chart uses the fixed 24-MAC scale and existing 60 windows. No new
  capture buffers. Both wip/unverified. C3 PASS (138.882s),
  `/tmp/x4-p6q-build.log`; image 6,464,672 bytes, OTA free 88,928, reserve warning.
  No deferred suites/device tests run. Vendor Lookup awaits absent external
  dataset versus ledger flash-table choice; Full Sweep/BLE gates stay open.
  Actionable P6 source work checkpointed; P7 next. Usage at prior boundary
  19% five-hour/35% weekly, no artificial usage stop.
  Device: global/local/multicast MACs, repeated/overflow observations, CSV
  escaping/SD errors, chart wrap/orientations, pause/resume and owner release.
  No cache reset. Unrelated work and shipping partitions preserved.

- 2026-09-24 16:09 UTC run, P7 Matrix Rain: reference falling-character
  effect ported with fixed ~3.4 KiB grid/column arrays, runtime safe-area layout,
  cosmetic local PRNG, translated UI, pause/density/speed controls and normal
  auto-sleep. Animation intervals 1/2/3 seconds; no busy-loop override or extra
  framebuffer. Source wip/unverified; wide screens may leave unused columns.
  C3 compile PASS (137.983s), `/tmp/x4-p7a-build.log`; image 6,467,440 bytes,
  OTA free 86,160, reserve warning remains. Usage boundary 36%/weekly 38%.
  No deferred suites/device tests run. PORT_NOTES records orientation, ghosting,
  input, pause, sleep, timing/battery and heap checks; no cache reset.
  P7 remaining ports next. Unrelated work/partitions preserved; no flashing.

- 2026-09-24 21:09 UTC run, P7 Voronoi: fixed 6,000-byte cell cache and
  320-byte points, orientation-aware step size within 100x60 cells, clipped
  edges, 5–40 points, explicit regeneration and dither/boundary rendering.
  No animation or extra framebuffer. Maximum 240,000 distance comparisons
  per generation; no measured performance claim. Source wip/unverified.
  C3 compile PASS (131.944s), `/tmp/x4-p7b-build.log`; image 6,469,536 bytes,
  OTA free 84,064, reserve warning remains. Usage 8% → 21% five-hour,
  41% → 43% weekly. No deferred suites/device tests. Other manual repo work
  not observed; unrelated files preserved. Device checklist in PORT_NOTES:
  both orientations, point-count bounds, repeated generation, clipped edges,
  dither visibility, input latency, auto-sleep and entry/exit heap. No cache
  reset, flashing or partition changes. Remaining P7 ports next.

- 2026-09-25 02:11 UTC run, P7 Chess: reference simplified game and random
  bot ported with 28-pair fixed move lists (228 bytes each), no recursive
  search, and single-move reservoir selection replacing 3,488-byte bot array.
  Rejects king capture, fails closed/logs missing king, serializes loop updates
  with rendering; normal auto-sleep and Back during bot wait. Setup discloses
  omitted castling/en passant/underpromotion/repetition/move-count/material
  draws. Source wip/unverified. C3 PASS (132.183s), `/tmp/x4-p7c-build.log`;
  image 6,476,192 bytes, OTA free 77,408, reserve warning remains. No deferred
  suites/gameplay/device tests run. Initial usage 9% five-hour/45% weekly.
  Device/V1: pins, king adjacency, check escapes, mate/stalemate, captures,
  promotion, human/bot/cancel/new game, orientations, stack/heap and worst-case
  move latency (PORT_NOTES). No cache reset. Unrelated work and partitions
  preserved; no other active repo task observed. Remaining P7 ports next.

- 2026-09-25 02:11 UTC run, P7 Screen Decoy: four translated cosmetic
  views with explicit selection/preview/activation and Back/Confirm exit. No
  lock, encryption, power, radio or storage changes; normal sleep may replace
  the view, disclosed before activation. Scalar state/flash key table only.
  Initial integration missed registry/keys; subsequent compile caught indexed
  tr macro misuse, corrected to I18N.get. Final C3 PASS (78.671s),
  `/tmp/x4-p7d-corrected-build.log`; image 6,478,912 bytes, OTA free 74,688,
  reserve warning remains. Source wip; deferred suites/device tests not run.
  Device: four views, preview/cancel/activate, blank-screen exit, orientations,
  sleep/wake/recovery and unchanged storage/radio/security state (PORT_NOTES).
  No cache reset or flashing. Unrelated files preserved. Remaining P7 next.

- 2026-09-25 07:11 UTC run, P7 Task Manager diagnostics: platform-isolated
  internal/PSRAM total/free/largest/low-water reads, absent/simulator metrics
  explicitly unavailable, five-second refresh, SD/display/boot ticks and
  managed-radio owner/duration. Fixed snapshot/48-byte owner buffer; no task
  termination, memory clearing, radio acquisition or writes. CPU/flash detail
  stays in base Settings. Independent readings/low-water caveats visible.
  Source wip/unverified. C3 PASS (203.701s including dependency refresh),
  `/tmp/x4-p7e-build.log`; image 6,482,144 bytes, OTA free 71,456, reserve
  warning persists. Usage 11% → 36% five-hour, 53% → 57% weekly. Deferred
  suites/device tests not run. No other active repo work observed; unrelated
  files/partitions preserved. Device: compare serial allocator readings,
  C3 absent/S3 separate PSRAM, SD/radio status, refresh/page/orientation/sleep
  and repeated entry/exit heap (PORT_NOTES). No cache reset or flashing.

- 2026-09-25 12:11 UTC run, P7 Network Monitor views: menu reuses owned
  Deauth monitor and WiFi Scanner with new shared-SSID observations. Forty-byte
  group index compares pre-sanitization SSIDs; hidden names stay separate,
  BSSIDs deduplicated within groups, channel/open-protected counts bounded by
  the 40-result snapshot. No rogue/attack attribution. Child allocation checked;
  parent owns no radio. Source wip: source/BSSID aggregation and rate graph next.
  C3 PASS (137.374s), `/tmp/x4-p7f-build.log`; image 6,485,280 bytes, OTA free
  68,320, reserve warning persists. Deferred suites/device/RF tests not run.
  Usage initially 55%/weekly 60%, then reset to 16%/weekly 62%; no usage
  deferral applied. No overlapping active repo task observed; unrelated work
  and partitions preserved. Device checklist in PORT_NOTES covers both modes,
  radio handoff/reuse, hidden/duplicate/same-display names, mixed security,
  capped scans, CSV, orientation and heap. No cache reset or flashing.

- 2026-09-25 17:12 UTC run, P7 Network Monitor summaries: fixed 24-key
  source/BSSID/subtype table with first/last processing uptime, latest metadata,
  saturating counts and untracked-event counter; 40 elapsed-time rate windows
  covering deauth/disassociation frames. Up toggles chart; Down cycles events;
  Page Back saves numbered exclusive CSV and acknowledges burst. Unknown or
  protected reasons export -1. Queue/untracked counts exported; no attribution.
  Fixed summary/rate storage adds approximately 2 KiB to shared monitor.
  Initial C3 compile found uint32_t/std::max mismatch; explicit type corrected.
  Final C3 PASS (84.420s), `/tmp/x4-p7g-final-build.log`; image 6,488,352 bytes,
  OTA free 65,248, reserve warning persists. Source wip; deferred suites,
  device/RF/SD checks not run. Usage 14% → 37% five-hour, 65% → 69% weekly.
  Device: repeat keys, >24 keys, subtype split, protected/short reasons,
  rate wrap/pause/channel hopping, CSV slots/SD failures, queue pressure and
  repeated entry/exit heap. No cache reset, flashing or partition changes.
  No other active repo task observed; unrelated files preserved.
