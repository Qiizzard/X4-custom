# Scheduled X4 merge batches

Requested 2026-09-05: start one bounded batch every five hours, targeting
roughly half the account's five-hour Codex allowance. This is a best-effort
usage target, not a guaranteed quota reservation or 2.5 hours of runtime.
No firmware work resumes until the scheduled wakeup.

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
- Preserve the five-hour cadence and existing approximate allowance limits:
  defer at starting usage >40% or weekly >=90%; target <=40 percentage points
  of work plus 10 checkpointing, stop at +50 or 85% total. If the window resets,
  checkpoint. Never buy/redeem credits. Do not overlap other active work.

## Active implementation queue

| Wave | Status | Work |
|---|---|---|
| P1 | implementation complete; validation deferred | Snake, Minesweeper, Tetris, Sudoku and Maze integrated (wip). Other approved games remain in P7. |
| P2 | implementation complete; validation deferred | Etch-A-Sketch, file-browser registration, Barcode and Key Bitting Charts integrated. |
| P3 | actionable implementation complete; validation deferred | Event Logger, Flashcards and Habit Tracker integrated (wip). Breadcrumb Trail/Vehicle Finder need product/location choices; Transit Alert awaits network infrastructure. |
| P4 | actionable implementation complete; validation deferred | Password Manager, Authenticator/TOTP QR and Stego Notes integrated (wip). Medical Card still needs its access policy. Hardware crypto/recovery gates remain unverified. |
| P5 | source implementation complete; validation deferred | Eight migration sites and Settings/KOReader-auth/OPDS/Calibre parents integrated (wip). Every picker caller passes a station token; legacy fallback removed. OTA/recovery and radio lifecycle hardware gates remain open. |
| P6 | pending | Approved network utilities and passive recon apps, sharing bounded radio/storage infrastructure. |
| P7 | pending | Remaining approved defense, comms, games and settings features; skip unresolved BLE/hardware/product choices and record them briefly. |
| V1 | deferred until ports finish | One consolidated host/simulator/soak/static-analysis/flash-budget and integration pass; fix failures together. |
| V2 | deferred until V1 | Produce test firmware and a concise hardware checklist; complete available device checks and record outstanding product/recovery gates. |

Start with the real source in ../biscuit-reference. P6 approved networking/passive recon source ports are next at the next eligible scheduled wakeup.
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
