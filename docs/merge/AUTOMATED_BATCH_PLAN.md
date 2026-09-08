# Scheduled X4 merge batches

Requested 2026-09-05: start one bounded batch every five hours, targeting
roughly half the account's five-hour Codex allowance. This is a best-effort
usage target, not a guaranteed quota reservation or 2.5 hours of runtime.
No firmware work resumes until the scheduled wakeup.

## Execution rules

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
| 5 | pending | Review the inherited compound-CSS fix, version-16 invalidation, fixture and cold/cache assertions. Add a focused missing regression only if needed, validate relevant builds and update cache/hardware instructions. No engine rewrite or runtime-font expansion. |
| 6 | pending | Close off-device flash-budget bookkeeping: reconcile actual full-image gate with stale documentation; add tested per-app flash attribution/planning support only if it is useful and clearly distinguishes estimates from linked image size. Keep shipping partitions unchanged. |
| 7 | pending | Review and locally commit verified work in coherent units (apps, CSS, navigation/SSID fixes, harness/docs as dependencies allow). Check formatting and relevant existing test/build evidence; rerun only checks invalidated by changes. Unverified implementations remain wip. Push the completed scoped commits. |
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
