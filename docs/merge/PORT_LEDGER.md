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
| Legacy radio migration | `wip` | Eight planned sites and all picker parents source-integrated; mandatory owner tokens. Runtime, device and OTA/recovery validation remain open. See `RADIO_MIGRATION.md`. |
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
| Clock | `wip` | Offline `HalClock` app, registered. C3 object measured at 312 bytes (2026-09-05). Simulator exercises only the unavailable-RTC fallback. Still needs populated-RTC date/time/offset verification. See `src/activities/apps/clock/PORT_NOTES.md` and the dated session report for current build and soak evidence. |
| QR Generator | `wip` | Registered keyboard/QR shell. C3 object measured at 488 bytes. Keyboard object allocation now checked. **Corrected from done:** prior ~100-second soak did not meet the 10-minute gate, and registry/entry soak remains on KeyboardEntry, not QR output. Scripted keyboard entry/output/edit-cancel/exit now pass. Initial cancellation also passes (batch 2). Phone scanning and transient keyboard/QR runtime allocation budget remain unverified; see verification/BATCH_2_QR_2026-09-08.md. See its PORT_NOTES and session report. |
| Cipher Tools (ROT13/Caesar/Vigenère/XOR/Atbash/Base64) | `wip` | Registered toy cipher tool; 16 host tests pass as part of the 246-test suite. C3 object measured at 664 bytes; keyboard allocations checked and algorithm labels translated. **Corrected from done:** earlier soak was only ~100 seconds; batch 3 now covers interactive input/key/result navigation, cancel and retry in simulator; physical UI and peak keyboard memory remain unverified. The current entry-screen soak is separate evidence. See its PORT_NOTES and session report. |
| OTP Generator | `wip` | Registered one-time-pad codebook generator, not a TOTP authenticator. C3 object measured at 832 bytes including mbedtls contexts. **Corrected from done:** simulator only exercises RNG-unavailable fallback; it does not test real page generation, entropy or hardware memory cleanup. Hardware generation/reseed and peak allocation checks remain. See its PORT_NOTES and session report. |

## Tools → Security & crypto

| App | Status | Notes |
|---|---|---|
| Authenticator (offline TOTP) | `wip` | Eight encrypted accounts, SHA-1/6/30 codes after per-boot NTP sync; shares vault lifecycle. Runtime crypto/time validation deferred. |
| TOTP QR | `wip` | Shared encrypted Authenticator accounts; reveals code-only QR, never seed. Phone/rollover/lifecycle validation deferred. |
| Password Manager | `wip` | Eight-entry encrypted vault in Tools: create/unlock, add/replace/delete, timed reveal and idle lock. Compile sanity only; recovery, crypto and lifecycle gates open. See secure_vault/PORT_NOTES.md. |
| Medical Card | `todo` | Deliberate design call needed: emergency data you cannot read without a PIN is not useful in an emergency. |
| Stego Notes | `wip` | Tools: bounded file selection, encrypted BMP note creation and timed reveal. Original preserved; concealment discoverable. Runtime/crypto/device validation deferred. |

## Tools → Network *(P5 source dependency integrated; device validation deferred)*

| App | Status | Notes |
|---|---|---|
| WiFi Connect | `wip` | Tools WiFi Networks entry reuses WifiSelectionActivity with an owned station hold, released at exit. Compile only; runtime validation deferred. |
| Host Scanner | `wip` | Local /24 slice or smaller IPv4 subnet, TCP 80 discovery, 32 records, selected-host 14-port check, CSV. Cancel between probes; excludes own/network/broadcast addresses. Runtime/heap/device validation deferred. |
| Ping | `wip` | Ping (TCP), matching Biscuit TCP-80 semantics honestly: five attempts, 1-second connect waits, success timing, bounded DNS input, named station hold. Not ICMP; device validation deferred. |
| DNS Lookup | `wip` | Tools single-host resolver, named station hold, ASCII hostname bound, IPv4/IPv6 result. SDK timeout/cache policy; runtime/device validation deferred. |
| HTTP Client | `wip` | GET/plain-text POST, 256-byte URL/512-byte body, fixed 1024-byte ASCII preview. TLS via `esp_crt_bundle_attach`; no redirects or embedded credentials. Runtime/TLS/resource/device tests deferred. |
| mDNS Browser | `wip` | Tools service selection, one 2-second query, up to eight fixed results, named station hold and per-query SDK cleanup. IPv4/IPv6 details and exclusive numbered CSV export integrated; all-services discovery integrated with between-query cancellation and partial-result labels. Runtime/resource/device validation deferred. |

## Tools → Tracking & logging

| App | Status | Notes |
|---|---|---|
| Event Logger | `wip` | Bounded 50-note ring, explicit-submit append, uptime timestamps. P3 compile only; V1 tests deferred. |
| Flashcards | `wip` | P3 bounded deck port; C3 compile only, full validation deferred. |
| Habit Tracker | `wip` | Explicit sessions, fixed habits and alternating checked saves; full validation deferred. |
| Breadcrumb Trail | `todo` | No GPS on this hardware — decide what it actually records before porting. |
| Vehicle Finder | `todo` | Same. |
| Transit Alert | `todo` | Needs network; blocked with the Network tile. |

## Tools → Creative

| App | Status | Notes |
|---|---|---|
| Etch-A-Sketch | `wip` | P2 source port: 4800-byte logical drawing, checked BMP export. Full tests deferred. |
| Barcode Generator | `wip` | Code 128B/Code 39/EAN-13 source port with bounded input. Compile sanity only; full validation deferred. |
| Key Copier | `wip` | Reference charts only, exposed as Key Bitting Charts. No key capture/import/save or calibrated dimensions. Validation deferred. |
| WiFi QR Share | `todo` | Renders a stored password as a QR. Confirm the vault gate before it can. |
| File Browser | `base` | Existing FileBrowserActivity registered in Tools during P2; preserves base book-browser behavior. New launcher route awaits V1 interaction checks. |

## Games

| App | Status | Notes |
|---|---|---|
| Dice Roller | `done` | Fixed array instead of a per-frame `push_back` vector; `esp_random` replaced with an in-object PRNG so it builds for the simulator too. |
| Snake | `wip` | Source port integrated in P1; compile sanity only, full validation deferred. |
| Minesweeper | `wip` | Source port integrated in P1; compile sanity only, full validation deferred. |
| Sudoku | `wip` | P1 source port integrated; compile sanity only, full testing deferred to V1. |
| Tetris | `wip` | Source port integrated in P1; compile sanity only, full validation deferred. |
| Maze | `wip` | P1 source port integrated; compile sanity only, full testing deferred to V1. |
| Game of Life | `wip` | Registered manual-step simulation with two 1,024-byte boards; C3 object measured at 252 bytes. Allocation failure now logged and buffers released in reverse order. Batch 4 simulator checks cover block/wrapped-blinker behavior, generation/population and restart; button step/restart/exit also pass. Physical display/input and C3 peak-memory checks remain. See its PORT_NOTES and session report. |
| Voronoi | `todo` | Watch the per-frame cost on a 160 MHz core. |
| Matrix Rain | `todo` | E-ink refresh cost — cap the frame rate or it is a battery bug. |
| Chess (with bot) | `todo` | Largest of the tile: search depth is a RAM/CPU budget question, not a feature question. |
| Casino (multi-mode) | `todo` | Play credits only. `casino.dat` never touches anything real — state that in its PORT_NOTES. |

## Recon *(passive only — listen, never transmit)*

| App | Status | Notes |
|---|---|---|
| WiFi Scanner | `wip` | Recon snapshot/detail view: passive scan, 40 fixed results, RSSI sort, manual rescan and 13-channel count/peak/mean view. Exclusive numbered CSV export integrated; BSSID-pinned 40-sample passive history integrated; runtime/device validation deferred. |
| Packet Monitor | `wip` | Fixed 2 KiB queue, frame/type/drop counts, channel/hop/pause controls, 96-byte-prefix PCAP up to 1 MiB per exclusive file. Dequeue uptime timestamps. Channel chart/CSV and 24 tracked source MAC summaries integrated; unequal channel dwell and queue drops remain explicit. Runtime/RF/SD gates open. |
| Probe Sniffer | `wip` | Passive request-frame count, eight recent source/SSID/RSSI/channel events, bounded parser and protected/truncated handling. 24 tracked source-MAC summaries with latest SSID/RSSI and CSV integrated; not a physical-device count. Runtime/RF validation deferred. |
| Deauth Detector | `wip` | Passive deauth/disassociation frame counts and eight recent events; no attack attribution or protected-reason decoding. Observed burst indicator (at least five processed deauth frames per roughly two-second interval), explicit elapsed time/acknowledgment; runtime/RF validation deferred. |
| Wardriving | `wip` | Passive scans, 64 tracked BSSIDs, first-observation CSV, 10-second scan pauses, 1 MiB log cap. Uptime only, no location. Runtime/SD/RF tests deferred. |
| AP History | `wip` | Passive repeated CSV observations, 40 results/64 tracked BSSIDs, selectable 1/5/10/30-minute scan pauses, bounded exclusive journals. Runtime/SD/RF tests deferred. |
| Network Change | `wip` | Capped passive baseline/current comparison, new/absent/metadata changes, versioned CRC-protected exclusive baseline slots and valid-file fallback. No offline-device claim; file/runtime/RF validation deferred. |
| Crowd Density | `todo` | |
| Device Fingerprint | `todo` | |
| Vendor Lookup | `todo` | OUI table must be `static const` in flash (rule 4), and it is large — budget it before porting. |
| WiFi Heat Map | `wip` | Passive RSSI CSV journal, 5-second scan pauses, 40 results/64 tracked BSSIDs, 1 MiB/10,000-row caps. No spatial coordinates; runtime/SD/RF unverified. |
| Signal Locator | `wip` | Three positions with three passive scan attempts each; averages only observed target BSSID samples and shows missing readings. No calculated location; runtime/RF unverified. |
| Perimeter Watch | `wip` | Volatile passive baseline up to 40 BSSIDs; bounded new-observation tracking within 64 total records, pause/resume and CSV export. No intrusion claim; runtime/SD/RF unverified. |
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

Latest verified full working-snapshot image (2026-09-24 UTC, P6 AP journals/snapshots):

| Quantity | Bytes |
|---|---:|
| Firmware image (`firmware.bin`) | 6,456,128 |
| Smallest configured OTA app slot | 6,553,600 |
| Free space | 97,472 |
| Warning reserve | 262,144 |
| Shortfall against reserve | 164,672 |

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

## Historical verification order — audit 2026-09-05

The active feature-first queue in AUTOMATED_BATCH_PLAN.md supersedes this
ordering as of 2026-09-09. These checks remain deferred release validation.

The intake ledger contained 12 `done`, 2 `wip`, 60 `todo`, 11 `base`, and
4 `blocked` rows (including foundations and reader rows, not just apps).
QR Generator, Cipher Tools and OTP Generator are restored to `wip`: the
previous 100-second holds did not satisfy the required ten minutes, and
entry-screen tests do not verify their main interactive/hardware paths.
The current counts are 9 `done`, 16 `wip`, 49 `todo`, 11 `base`, 4 `blocked`.
Prior `done` rows are retained as historical gate records, not newly certified.

1. Complete the five current ports' physical and resource checks in PORT_NOTES.
   QR/Cipher button paths and Game of Life step/restart/counter contracts now
   pass in the simulator. Phone decoding, physical display/input, RTC-equipped
   device behavior, actual OTP entropy and C3 peak allocations remain open.
2. Reader: compare images, justification, margins and line spacing at every
   shipped font size, cold versus cached, on the physical X4. Simulator CSS
   assertions verify rule lookup only, not e-ink typography.
3. Security order remains Authenticator → TOTP QR → Password Manager →
   Medical Card → Stego Notes. Before a SecureStore consumer ships, complete
   ACCEPTANCE.md's C3 KDF timing and 50-cycle crypto memory gate. Medical
   Card's emergency accessibility is a product decision; do not invent it.
4. Complete PARTITION_DECISION.md's five-step physical recovery gate before
   expanding the app set. The shipping table remains unchanged. The initial
   build in this audit had 238,064 bytes free, below the 262,144-byte reserve;
   see the session report for the final build measurement.
5. Radio migration remains one call site at a time, ClockSync first and OTA
   last, with per-site physical teardown/next-screen checks. No migration
   or hardware verification is claimed by this audit.
6. Resume remaining tiles in their existing order after these dependencies
   clear. `todo` still means unfinished, not permanently rejected. Network
   apps depend on radio migration; BLE apps require a separate scope decision.

`scripts/check_firmware_size.py` already checks the actual image against the
smallest configured app partition and warns below the reserve (commit
`f5b4ea1a`). The RAM manifest is an estimate/planning gate, not a compiled-app
inventory; controlled per-app image deltas are the planning method once
the expansion gates clear, not an unimplemented replacement for the image gate.
Use a space-free copy for device builds on this Mac, per AGENTS.md. The
previous successful spaced-path report does not override that instruction.

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

## Batch 1 verification — 2026-09-06

All five current wip ports passed the full 50-cycle / ten-minute entry-screen
lifecycle soak; final cleanup passed within 4096 bytes of Home baseline.
Evidence: `verification/BATCH_1_SOAK_2026-09-06.md`. This supersedes the
interrupted-run checkpoint, not the separate interaction/hardware gates.
All five app statuses remain wip.
