# GOVERNANCE — how this firmware stays reliable

The failure mode of an ambitious fork is scope. biscuit shows what happens when
features outrun discipline: ~130 apps and a 91-finding audit. This project's one
structural defense is that **scope is governed, not vibed.**

## The bar every feature clears
Before an app is added or ported, it must answer yes to all five:
1. Does it work **offline**?
2. Does it **fit the C3 budget** (or honestly declare `tier: psram`)?
3. Is it operable on **7 buttons**, no touch?
4. Does it **respect the radio** (RadioManager, acquire/release)?
5. Does it **store data responsibly** (under `/`-scoped SD paths, encrypted if sensitive)?

If any answer is no, it doesn't go in — or it goes in changed until the answer is yes.

## The gate
No app merges without a green `PORT_CHECKLIST.md` and a `budget_check.py` PASS.
This applies to **our own additions too** — the gate is not just for ported code.
CI enforces the budget and static analysis; the checklist is human sign-off.

## Upstream flow (one-directional)
- `crossink` and `crosspoint` are tracked as read-only upstream remotes.
- We **pull** their fixes/releases downstream and resolve conflicts preserving
  upstream intent (see the merge-conflict rule in the base repo's guide).
- We do **not** push back: CrossInk does not accept PRs. Genuinely upstream-worthy
  fixes should be offered to CrossPoint directly.

## Scope ledger
What is in / out, so "why isn't X in here" has a standing answer. The
authoritative per-app status lives in `docs/merge/PORT_LEDGER.md`; this is the
summary.

**Correction (v2, 2026-08-30).** The v1 entry here claimed "21 apps + reader,
all gate-passed" and three landed foundations. That was not true of the tree:
`lib/RadioManager`, `lib/RingBuffer` and `lib/SecureStore` did not exist, and
the only app present was `apps-ported/calculator/`, in a directory the firmware
does not compile. What v1 actually delivered was the governance layer — this
file, `RULESET.md`, `PORT_CHECKLIST.md`, `PORT_GUIDE.md`, `ACCEPTANCE.md`,
`SECURITY.md`, the budget checker and the PR template — which is real and is
what v2 builds on. The ledger is corrected rather than quietly restated because
a gate whose own records are aspirational cannot gate anything.

**Landed (v2, gate-passed):**
- **Foundations** — `lib/RingBuffer` (rule 10 SPSC primitive, 11 host tests),
  `lib/SecureStore` (rule 22 AES-256-GCM + PBKDF2, 19 host tests),
  `lib/RadioManager` (rules 7-9 arbitration). All three compile into
  `-e default` and `-e simulator` and are covered by CI.
- **Tools launcher** — `AppRegistry` (constexpr table in flash, function-pointer
  factories) + `AppLauncherActivity`, reached from Home. Reading stays the home
  screen; the tile grid is one press from it.
- **Apps ×5** — Calculator, Unit Converter, Morse Code, Countdown (Tools);
  Dice Roller (Games). All safe tier, no radio, budgets measured not estimated.
  The two with real logic (Morse, Unit Conversion) are split Arduino-free and
  carry 28 host tests between them.
- **Reader** — CrossInk's, unchanged. Verified by the simulator smoke test.

**Already in the base, not ports.** Three of v2's headline reader items turned
out to be implemented in CrossInk already: the single-pass structural cache
(`book.bin` + `sections/*.bin` + the inflated-HTML cache that lets a font or
size change re-flow without re-reading the zip), Liang/TeX hyphenation across
ten languages with tries in flash, and true bold/italic faces. Seven of the
eight Settings-tile "apps" likewise already exist. These are verified and
registered, never rewritten — reimplementing working reader code is the one
change with a guaranteed downside and no upside.

**Deliberately deferred / cut:**
- **Offense / active-attack suite** and the Capture tile that holds its output —
  **cut, permanently**, not deferred. The boundary is behavioural: nothing here
  transmits a spoofed AP, injects keystrokes, or harvests credentials.
- **XOR "obfuscation"** (`lib/Serialization/ObfuscationUtils`) — banned by rule
  22 and superseded by `SecureStore`, but **still present and still in use** by
  `WifiCredentialStore` and `OpdsServerStore`. Not yet removable: those
  credentials must be readable unattended at boot, so they cannot be keyed on a
  user passphrase. Resolving that is a design decision, tracked in `SECURITY.md`.
- **BLE apps** (BLE Scanner, BLE Proximity, Tracker Detector) — blocked, not
  deferred by preference: this tree has no BLE stack (`lib_ignore = BLE`).
  Adding one is a real RAM cost on the C3 and must clear the gate on its own.
- **Clock with NTP** — still radio-tier; the safe-tier version was rejected in
  v1 for pulling in `<WiFi.h>` (the gate working as intended).
- **Full-frame packet capture** — X4 Pro / PSRAM only; the C3 ships a
  snap-length monitor.

**Standing constraint: flash, not RAM — and the scope does not currently fit.**
`-e default` links at 95.9% of the OTA app partition (6,287,531 of 6,553,600
bytes; 251,744 free). RAM is comfortable at 17.6%.

The first wave measured the real cost: **~4 KB per app**, plus 18 KB one-time
for the launcher. About 68 apps remain in scope, i.e. ~272 KB against 251 KB
free — and 4 KB is the average of the five *simplest* apps on the list. **The
~80-app scope does not fit the current partition table.**

This is a governance question, not just an engineering one, so it is recorded
here: the table reserves 3.4 MB for a filesystem partition nothing mounts, and
reclaiming it would cover the whole scope. That is an OTA-layout change under
rule 23 and must be its own change with the rollback tested on hardware. Until
it is resolved, treat every new app as competing for 251 KB, and add a
`flash_bytes` column to `app_budgets.yaml` — the gate measures RAM only today
and would pass a change that cannot be flashed. Full arithmetic and the
alternatives are in `docs/merge/PORT_LEDGER.md`.

## Naming
The firmware needs its own name and a visible "based on CrossInk / CrossPoint"
credit (see `NOTICE`). Working name: **x4-merge** — replace before first release.
