# ACCEPTANCE — on-device v1.0 sign-off

> **Status note (v2).** This checklist was written ahead of the apps it tests.
> The Launchers, Tools/Games, Recon, Comms/Defense and Security sections below
> reference apps that are **not in the tree yet** — see `PORT_LEDGER.md` for what
> actually exists. Treat those sections as the acceptance criteria each app must
> meet when it lands, not as a list you can work through today. The Boot & reader
> and Recovery sections are runnable now, and the v2 foundations section further
> down is the part that applies to what has shipped.

Run this on your **unlocked** X4 after flashing. This is the last mile that can only
be done on hardware. Don't call v1.0 done until every box is checked. Any watchdog
reset, boot loop, or heap that doesn't return to baseline = **not ready**.

Watch free heap via serial (`ESP.getFreeHeap()`), or the device-info screen.

## Boot & reader (the ground)
- [ ] Boots to Home within a few seconds; battery + status render.
- [ ] Open an EPUB, turn pages, change font/size — reading works as on stock CrossInk.
- [ ] Sleep/wake holds the image and resumes.

## Launchers
- [ ] Home shows **Tools, Games, Recon, Comms, Defense** plus the reading items.
- [ ] Each launcher opens its list; **Back** returns Home.
- [ ] Selecting an app opens it; leaving it returns to the list (not Home).

## Tools (9) & Games (6)
- [ ] Spot-check Calculator (math correct), Sudoku (solve/validate), Tetris (plays), QR (renders).
- [ ] Open & close ~20 apps in a row — **free heap returns to baseline** (no leak).

## Recon (radio — the reliability crux)
- [ ] WiFi Scanner lists APs (≤40), rescans; on exit the next radio app still works (radio released).
- [ ] BLE Scanner lists devices; opening a device's services/characteristics doesn't crash.
- [ ] Packet Monitor: start capture → channel hops → a `.pcap` appears under `/biscuit/pcap/`.
- [ ] **Packet Monitor 10-minute soak** under real traffic: no watchdog reset, heap stable.
- [ ] The captured `.pcap` opens in Wireshark (frames present, truncated at snap len is fine).

## Comms & Defense
- [ ] Mesh Chat: two X4s discover each other and exchange a message; **10-minute soak** stable.
- [ ] Tracker Detector runs a BLE sweep and flags/handles repeats; exits cleanly (radio released).
- [ ] Deauth Detector runs; trigger a test deauth if you can; exits cleanly.

## Security (the whole point of the fix)
- [ ] Set a real PIN **and** a duress PIN. Add a password entry, lock (sleep/exit).
- [ ] Unlock with the **real** PIN → your entry is there.
- [ ] Unlock with the **duress** PIN → a **decoy** vault (empty/planted); the real entry is NOT shown.
- [ ] Inspect `passwords.json` on the SD card — it's ciphertext; no username/password in the clear.

## Recovery (do once, so you trust it)
- [ ] Confirm SD System Update works, and that restoring a stock `update.bin` boots stock.

---

# ACCEPTANCE — v2 foundations

Added when `lib/RingBuffer`, `lib/SecureStore` and `lib/RadioManager` landed.
These are libraries, not screens, so most of their behaviour is already proven
on the host by `ctest` (11 + 19 cases, in CI). What follows is only what a host
test **cannot** tell you: real timing on a 160 MHz core, real RF, and real
teardown between screens.

Sign these off before the first app that depends on them ships. Until then the
firmware links them but nothing exercises them on hardware, and the ledger says
so.

## Before you start
- [ ] `-e default` flashed; free heap noted from the device-info screen (baseline).
- [ ] A stock `update.bin` is on hand for rollback (rule 23) before any radio work.

## SecureStore — KDF cost on real silicon
The one number that cannot be guessed. `kDefaultIterations` is currently 50,000,
chosen to be defensible but **unmeasured on device**.

- [ ] Time one `securestore::decrypt()` of a ~4 KB blob at 50,000 iterations on
      the C3. Record the milliseconds here: __________
- [ ] If it is far from ~500 ms, retune `kDefaultIterations` so an unlock lands
      near half a second, and re-record. Too fast is weak; too slow makes people
      pick shorter PINs, which is worse.
- [ ] Confirm old blobs still open after retuning — each blob records the count
      it was written with, so raising the default must not orphan existing
      vaults. (Write one at the old value, raise the default, reopen it.)
- [ ] Watch free heap across 50 encrypt/decrypt cycles: returns to baseline, no
      downward drift. The mbedtls contexts are heap-allocated per call by design
      (rule 15 stack budget); this is the check that they are all freed.

## RadioManager — the leak this exists to prevent
Every check here is really the same check: **does the next screen still work?**

- [ ] `acquire()` → `shutdown()` in one app, then open a CrossInk network screen
      (OPDS or File Transfer): it connects normally. A dead antenna here means
      `stopWifi()` is not taking the radio all the way down.
- [ ] The reverse: connect WiFi from a CrossInk screen, leave it connected, then
      open an app that calls `acquire()`. It must be **refused** with a "radio
      busy" state — not take the antenna. Confirm the original connection
      survives (`foreignRadioActive()` doing its job).
- [ ] Two radio apps in sequence: the second acquires successfully after the
      first exits.
- [ ] Exit a radio app via **every** path — Back, sleep, and a Home gesture —
      and confirm the radio is released each time. Serial shows `released` with
      a hold duration. Only Back is easy to get right; the others are where a
      leak hides.
- [ ] Leave a radio app running 10 minutes: no watchdog reset, heap stable.
- [ ] `scanNetworks()` in a dense environment: at most 40 results, no crash, and
      the serial line reporting the cap appears when more APs are present.

## RingBuffer + promiscuous path — under real traffic
Needs a capture app, so this is deferred until the first one lands. Recorded now
so it is not rediscovered later:

- [ ] Promiscuous capture under real traffic for 10 minutes: no watchdog reset,
      heap flat (the callback must allocate nothing).
- [ ] Drop counter is non-zero under load **and** the UI shows it. A capture that
      claims zero drops on a busy channel is not being honest.
- [ ] Channel hopping while capturing does not tear or reorder frames.

## Launcher and the first app batch

Everything below is already exercised in the simulator on every CI run (the
smoke test walks the registry and renders each app). These are the checks a
desktop cannot make: real buttons, real e-ink refresh, real heap.

- [ ] Home shows **Tools** between File Transfer and Settings. Selecting it
      opens the tile grid; **Back returns to Home**, not to the reader.
- [ ] The grid lists Tools, Games, Recon, Defense, Comms with an app count
      beside each. Recon/Defense/Comms read 0 and open to "No apps here yet" —
      an empty tile must say so rather than show a blank screen.
- [ ] Open each of the five apps and return: Calculator, Unit Converter,
      Morse Code, Countdown (Tools); Dice Roller (Games).
- [ ] Open and close the launcher and all five apps **20× in a row** — free heap
      returns to baseline. Four of the five allocate nothing at all, so any
      drift here points at the launcher or the activity teardown.

### Per-app correctness (the parts a screenshot cannot confirm)
- [ ] **Calculator**: `7 × 8 =` gives 56; the history strip keeps the last
      entries; `C` clears without leaving a stale expression.
- [ ] **Unit Converter**: Temperature → Celsius, enter `100` → 212 °F and
      373.15 K. Enter `-40` → −40 °F. Data → Byte, enter `1` → TB shows a small
      non-zero value, **not** `0`.
- [ ] **Morse Code**: `SOS` → `... --- ...`. Swap direction and confirm it
      decodes back. A long sentence shows "Too long to convert" rather than a
      silently shortened code.
- [ ] **Countdown**: ticks once per second (not a full refresh per loop); Select
      pauses and resumes without losing time; **the screen does not sleep while
      running**, and does sleep on the picker; stops at 00:00.
- [ ] **Dice Roller**: the roll animation runs and settles; the total matches
      the dice shown; 6 × d100 renders without clipping.

### Button grammar (rule 20 — the same button does the same kind of thing)
- [ ] In every app, Back exits (or steps back one level) and Select confirms.
- [ ] On a touch device, the header back button works in every app.

## Reader — the thing that must never regress
Run this after any change in this wave, foundations included.

- [ ] Open an EPUB, turn pages, change font **and** size, close it. Layout is
      correct at every size.
- [ ] Reopen the same book: it is fast (the cached structure and inflated HTML
      are being reused, not re-parsed).
- [ ] A hyphenated language book (de/ru/fr) still hyphenates.
- [ ] Free heap after closing returns to the pre-open baseline.

## Open reader item from the v2 scope
The one reader gap that is genuinely unverified (`PORT_LEDGER.md`):

- [ ] At **every** shipped font size (10/12/14/16 pt), confirm inline images and
      justification/margins/line-spacing render identically on a cold parse and
      on the cached path. Clear `.crosspoint/epub_<hash>/` between the two.

---
When every box is checked, tag **v1.0** and update `CHANGELOG.md`. Until then it's a
release candidate — a good one, but hardware is the judge.
