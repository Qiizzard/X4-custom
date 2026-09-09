# PORT LEDGER — the full v2 scope, and where each item actually stands

The running answer to "what's left", so no session has to reconstruct it from
context. One row per app in the v2 scope table. Update it in the same change
that lands the app — a row that says `done` and a tree that does not build it
are how v1 ended up claiming 21 apps it did not have.

**Status values**

| Status | Means |
|---|---|
| `done` | In `src/`, compiles on `-e default` **and** `-e simulator`, `PORT_CHECKLIST.md` green, budget PASS, registered in the launcher. |
| `wip` | Started, not through the gate. Never leave a session on `wip` without saying so in `CHANGELOG.md`. |
| `blocked` | Needs a decision or a dependency named in the Notes column. |
| `todo` | Not started. |
| `base` | Already shipped by CrossInk. Not a port — verify and register, do not rewrite. |

**Order.** Tile by tile, top to bottom, safe tier before radio tier, per
`PORT_GUIDE.md`. A session that runs out of runway partway down still leaves a
coherent, working subset.

---

## Foundations (nothing above the reader ships without these)

| Item | Status | Notes |
|---|---|---|
| `lib/RingBuffer` | `done` | Rule 10 primitive. Header-only SPSC framed ring, in-object storage, drops-and-counts on overflow. 11 host tests inc. a concurrent producer/consumer. |
| `lib/SecureStore` | `done` | Rule 22 crypto: AES-256-GCM + PBKDF2-HMAC-SHA256, per-blob random salt/IV, header authenticated as AAD. 19 host tests inc. single-bit tamper sweep + duress-vault separation. |
| `lib/RadioManager` | `done` | Rules 7-9 arbitration: one owner, symmetric acquire/shutdown, capped scan, listen-only promiscuous. Simulator builds a no-radio stub. **Not yet exercised on hardware.** |
| Legacy radio migration | `todo` | Eight CrossInk network call sites still drive Arduino WiFi directly. `acquire()` detects and refuses rather than stomping. See `RADIO_MIGRATION.md`. |
| App registry + Tools launcher | `done` | `src/activities/apps/AppRegistry.{h,cpp}` (constexpr table in flash, function-pointer factories) + `AppLauncherActivity` (two-level list on the shared FreeInkUI components). Reached from Home → Tools. |

## Reader / EPUB engine

| Item | Status | Notes |
|---|---|---|
| Structural cache (single parse, re-flow on font/size change) | `base` | Already implemented: `book.bin` v9 (spine/ToC), `css_rules.cache`, `sections/*.bin` keyed on `readerRenderSpecSignature()`, and `html/<spine>.html` so a font/size change re-flows **without re-inflating the zip** (`Section::hasHtmlCache()`). Verify, don't rebuild. |
| Liang/TeX hyphenation | `base` | Already implemented: `lib/Epub/Epub/hyphenation/`, 10 languages (en de es fr it pl pt ru sv uk), tries `constexpr uint8_t[]` in DROM (rule 4 satisfied), language from EPUB `dc:language` via `Hyphenator::setPreferredLanguage()`. Host F1 regression suite at 94-99%. |
| True bold/italic (not synthetic slant) | `base` | `EpdFontFamily` carries four real faces (regular/bold/italic/bold-italic). |
| Inline images + justification through the cache path | `todo` | The one genuinely open reader item: verify at every shipped font size that images and justification/margins/line-spacing apply identically through the cached path and a cold parse. Hardware check, not new code. |

## Tools → Productivity

| App | Status | Notes |
|---|---|---|
| Calculator | `done` | Moved from `apps-ported/` into `src/`. Re-audit found five untranslated user-facing strings the v1 PORT_NOTES had recorded as clean; fixed, and the header now uses the shared touch-aware component. |
| Unit Converter | `done` | Tables moved to flash; temperature constants corrected to exact fractions (biscuit's rounded values did not round-trip). 13 host tests. |
| Morse Code | `done` | Encode/decode split out Arduino-free; refuses to truncate rather than emit a wrong message. 15 host tests. |
| Countdown | `done` | Repaint gated on the seconds digit; holds the device awake only while actually running. |
| Clock | `blocked` | v1 rejected it for pulling in `<WiFi.h>` for NTP. Ship offline-only against the RTC, or gate NTP behind `RADIO`. |
| QR Generator | `todo` | `ricmoo/QRCode` is already a dependency. |
| Cipher Tools (ROT13/Caesar/Vigenère/XOR) | `todo` | Label it a **toy**, not protection — rule 21. It must not look like SecureStore. |
| OTP Generator | `todo` | |

## Tools → Security & crypto

| App | Status | Notes |
|---|---|---|
| Authenticator (offline TOTP) | `todo` | Seeds are secrets: store via `SecureStore`. |
| TOTP QR | `todo` | |
| Password Manager | `todo` | `SecureStore`'s first real consumer. |
| Medical Card | `todo` | Deliberate design call needed: emergency data you cannot read without a PIN is not useful in an emergency. |
| Stego Notes | `todo` | Steganography is concealment, not encryption. Must say so, or wrap the payload in `SecureStore`. |

## Tools → Network *(all `blocked` on the legacy radio migration)*

| App | Status | Notes |
|---|---|---|
| WiFi Connect | `base` | `WifiSelectionActivity` already exists. Register, don't rewrite. |
| Host Scanner | `todo` | |
| Ping | `todo` | |
| DNS Lookup | `todo` | |
| HTTP Client | `todo` | TLS via `esp_crt_bundle_attach` only. `setInsecure()` is banned (rule 22). |
| mDNS Browser | `todo` | |

## Tools → Tracking & logging

| App | Status | Notes |
|---|---|---|
| Event Logger | `todo` | Rule 8: debounce SD writes. |
| Flashcards | `todo` | |
| Habit Tracker | `todo` | |
| Breadcrumb Trail | `todo` | No GPS on this hardware — decide what it actually records before porting. |
| Vehicle Finder | `todo` | Same. |
| Transit Alert | `todo` | Needs network; blocked with the Network tile. |

## Tools → Creative

| App | Status | Notes |
|---|---|---|
| Etch-A-Sketch | `todo` | |
| Barcode Generator | `todo` | |
| Key Copier | `todo` | Bitting charts only. Nothing that reads or emits a credential. |
| WiFi QR Share | `todo` | Renders a stored password as a QR. Confirm the vault gate before it can. |
| File Browser | `base` | `FileBrowserActivity` already exists. |

## Games

| App | Status | Notes |
|---|---|---|
| Dice Roller | `done` | Fixed array instead of a per-frame `push_back` vector; `esp_random` replaced with an in-object PRNG so it builds for the simulator too. |
| Snake | `todo` | |
| Minesweeper | `todo` | |
| Sudoku | `todo` | |
| Tetris | `todo` | |
| Maze | `todo` | |
| Game of Life | `todo` | |
| Voronoi | `todo` | Watch the per-frame cost on a 160 MHz core. |
| Matrix Rain | `todo` | E-ink refresh cost — cap the frame rate or it is a battery bug. |
| Chess (with bot) | `todo` | Largest of the tile: search depth is a RAM/CPU budget question, not a feature question. |
| Casino (multi-mode) | `todo` | Play credits only. `casino.dat` never touches anything real — state that in its PORT_NOTES. |

## Recon *(passive only — listen, never transmit)*

| App | Status | Notes |
|---|---|---|
| WiFi Scanner | `todo` | `RADIO.scanNetworks()` already caps at 40. |
| Packet Monitor | `todo` | `RADIO.startPromiscuous()` + `RingBuffer`. Snap length, PCAP streams to SD. |
| Probe Sniffer | `todo` | |
| Deauth Detector | `todo` | Counts deauth frames. Detection only. |
| Wardriving | `todo` | |
| AP History | `todo` | |
| Network Change | `todo` | |
| Crowd Density | `todo` | |
| Device Fingerprint | `todo` | |
| Vendor Lookup | `todo` | OUI table must be `static const` in flash (rule 4), and it is large — budget it before porting. |
| WiFi Heat Map | `todo` | |
| Signal Locator | `todo` | |
| Perimeter Watch | `todo` | |
| Full Sweep | `todo` | Composite; port after its parts. |
| BLE Scanner | `blocked` | **No BLE stack in this tree.** `lib_ignore = BLE`; nothing links NimBLE. Adding it is a real RAM cost on a C3 and needs a gate decision. |
| BLE Proximity | `blocked` | Same. |

## Defense

| App | Status | Notes |
|---|---|---|
| PIN Security (with duress vault) | `todo` | `SecureStore` proves the duress separation cryptographically; the redirect still has to be wired and, per rule 22, actually invoked. |
| Screen Decoy | `todo` | |
| Quick Wipe | `todo` | Destructive. Needs a confirm flow and must never touch the OTA/SD recovery path (rule 23). |
| SD Encryption | `todo` | Scope it honestly: per-file via `SecureStore`, not "full-disk". |
| Security Sweep | `todo` | |
| Network Monitor | `todo` | |
| Ghost Mode | `todo` | Define precisely what it disables; must not disable the recovery path. |
| Emergency SOS | `todo` | Decide what it can actually do with no cellular radio. |
| Phone Tether | `todo` | |
| Tracker Detector | `blocked` | BLE. |

## Comms

| App | Status | Notes |
|---|---|---|
| Mesh Chat | `todo` | ESP-NOW; `RADIO.acquire(Mode::EspNow, ...)`. CrossInk's nearby-sync screens are the working reference. |
| Contact Exchange | `todo` | |
| Dead Drop | `todo` | |
| Bulletin Board | `todo` | |
| SSID Channel | `blocked` | **Out of scope as usually implemented.** Encoding data into a broadcast SSID means transmitting a crafted AP beacon, which is the Offense boundary. Ship only if a listen-only design exists. |

## Settings

| App | Status | Notes |
|---|---|---|
| Settings | `base` | `SettingsActivity`. |
| WiFi Transfer | `base` | `CrossPointWebServerActivity`. |
| USB Storage | `base` | `UsbSerialFileTransfer`. |
| Battery | `base` | Settings > Device. |
| Device Info | `base` | Settings > Device. |
| Background | `base` | Sleep-screen settings. |
| Task Manager | `todo` | Genuinely new. `RADIO.holdReport()` gives it the radio row for free. |
| Automation | `todo` | Genuinely new. Scope it against `SCOPE.md` before building. |

**No X4 Pro firmware target exists.** `platformio.ini` has `x4-pro-simulator`
but no `x4-pro` build env, so the X4 Pro is a supported device in the docs with
nothing to flash. `AGENTS.md` documented `pio run -e x4-pro` as a validation
command; that has been corrected. Adding the target is its own piece of work
(board profile, SDMMC storage, `FREEINK_FB_PSRAM`) and needs the hardware to
verify, so it is recorded here rather than guessed at.

---

## The flash budget — current gate and measured headroom

Latest verified full working-snapshot image (2026-09-08, batch 5):

| Quantity | Bytes |
|---|---:|
| Firmware image (`firmware.bin`) | 6,316,256 |
| Smallest configured OTA app slot | 6,553,600 |
| Free space | 237,344 |
| Warning reserve | 262,144 |
| Shortfall against reserve | 24,800 |

The image fits, but the low-headroom warning remains. This measurement
includes the inherited local app work; it is not a measurement of a clean
published commit or evidence that every app is ready to ship.

`scripts/check_firmware_size.py` is already wired into PlatformIO. It compares
the complete image to the smallest app partition in the configured CSV,
fails on overflow, and warns below `custom_flash_reserve_bytes` (256 KiB).
`CROSSINK_FLASH_FAIL_UNDER_RESERVE=1` makes that reserve a hard failure.
The RAM manifest is a separate estimate gate; it does not substitute for
this linked-image check. See verification/BATCH_6_FLASH_2026-09-09.md.

Historical launcher-wave measurements were 6,262,592 → 6,301,856 bytes
(+39,264 including launcher, registry, five apps and translations). The
old ~4 KB/app object-file average is planning context only. Shared code,
linker garbage collection, constants, alignment and configuration mean
neither that average nor summed object sizes predict total firmware growth.
The remaining scope is not demonstrated to fit; no claim that a proposed
partition guarantees it fits should be inferred from that average.

Before further app expansion, resolve the documented physical partition/
stock-recovery gate or choose a bounded optional-build/scope alternative.
Shipping partitions remain unchanged. A proposed partition is not permission
to activate it, and passing an image-size check is not recovery verification.

For a useful per-app estimate after those gates clear, build before/after
images with identical compiler, dependency versions, target, partition table
and flags, varying only the reviewed app integration. Record both hashes and
byte sizes and the delta; keep any shared-dependency costs explicit. Such
deltas are configuration-specific and not additive across apps. No invented
`flash_bytes` values are added to the RAM manifest in this batch.

## Next session starts here

The launcher and the first app batch are in. The next unit of work is
**resolving the flash budget above**, then continuing down the tiles.

1. **Settle the partition question above.** Everything else is blocked on it.
2. **Continue the Tools tile**: QR Generator (`ricmoo/QRCode` is already a
   dependency), Cipher Tools, OTP Generator, then Clock against the RTC.
3. **Then Games**, which are self-contained and need no new subsystems.
4. **Then the radio tier**, which starts with the `RADIO_MIGRATION.md` work
   rather than with an app.

**Adding an app now costs one row.** `src/activities/apps/AppRegistry.cpp` holds
a `constexpr` table of `{category, StrId, factory}`; add the row, add
`STR_APP_*` to `lib/I18n/translations/english.yaml` (English only — the
generator falls back for the other 27), and the launcher, the smoke test and the
budget gate all pick it up. The simulator smoke test walks the registry, so a
new app is entered and rendered on every CI run without touching the harness.

**Before starting, read the flash arithmetic below.** It is now measured, and
it changes the plan.

**Build note (macOS):** ESP-IDF refuses any project path containing a space, so
`-e default` cannot build from this checkout in place. Copy to a space-free path
to verify. `-e simulator` and the `ctest` suite build fine where they are.

## Explicitly out of scope — do not port, do not reimplement

biscuit's **Offense** tile and the **Capture** tile that exists to hold its
output: Beacon Flood, SSID Clone / evil twin, Captive Portal, Credential Viewer,
BLE Spam, BLE/USB Keyboard DuckyScript injection, AirTag Test, Vuln Assessment,
Target Profiler / Client Enum.

The line is behavioural, not a list of filenames: **nothing in this firmware
transmits a spoofed AP, injects keystrokes into another device, or harvests
credentials.** An app that would do any of those is out even if it appears
somewhere in the scope table under a friendlier name — `SSID Channel` above is
exactly that case.
