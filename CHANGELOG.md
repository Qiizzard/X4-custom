## [Unreleased] — x4-merge v2

The X4 merge builds a device firmware on top of CrossInk, pulling apps in from
biscuit (MIT) through the gate in `docs/merge/`. microreader (GPL v2) is a
reference only: none of its code is in this tree and none ever will be — the
base is MIT.

### Added

- Task Manager diagnostics (2026-09-25): separate internal RAM/PSRAM metrics,
  SD readiness, display dimensions and managed-radio ownership. Read-only,
  with five-second refresh; source wip, hardware verification deferred.

- Screen Decoy (2026-09-25): preview and activate four cosmetic views, with
  explicit exit controls and no locking/encryption/shutdown claim. Normal
  sleep may replace the view; source wip, device validation deferred.

- Simplified Chess (2026-09-25): two-player and random-move bot modes, bounded
  move lists, king-safety checks and explicit rule limitations. Source wip;
  gameplay/device verification deferred.

- Voronoi (2026-09-24): generate dithered nearest-point regions with 5–40
  points and orientation-aware bounded cells. Source wip; device validation
  remains deferred.

- Matrix Rain (2026-09-24): falling-character animation with density/speed
  controls, pause/resume and normal auto-sleep. Animation capped at one frame
  per second; wip pending device display, timing and battery validation.

- Device Fingerprint observation view and Crowd Density chart (2026-09-24):
  inspect capped probe-source metadata and MAC administration bits, with OS
  explicitly unknown. Both remain wip pending device validation.

- Crowd Density observation history (2026-09-24): bounded probe-source MAC
  counts over 60 processing windows, with sampling limits shown. These are
  not estimates of people or physical devices; wip, device validation deferred.

- Signal Locator, WiFi Heat Map logging and Perimeter Watch (2026-09-24):
  compare three RSSI sample positions, save bounded RSSI journals, and track
  BSSIDs observed after a baseline. No location or intrusion inference; device
  validation remains deferred.

- AP History, Wardriving and Network Change (2026-09-24): bounded passive
  journals and persisted snapshot comparisons. Logs use uptime, not location;
  missing BSSIDs are absent observations, not proof of an offline device.

- Passive monitor summaries (2026-09-24): channel chart/CSV, up to 24 tracked
  source MACs, probe summaries/CSV and an observed deauth burst indicator.
  Counts are capped observations; validation remains deferred.

- Passive Packet Monitor, Probe Sniffer and Deauth Detector cores (2026-09-24):
  bounded frame queue, recent events, channel controls and capped PCAP recording.
  Observed frames are not proof of an attack; device/RF validation deferred.

- HTTP Client (2026-09-24): explicit GET/plain-text POST, validated HTTPS and
  a bounded ASCII response preview. Network/TLS/device validation deferred.

- Ping (TCP) and Host Scanner (2026-09-24): bounded connection timing, local
  IPv4 discovery, selected-host port checks and CSV export. TCP results are
  not ICMP replies or proof that nonresponding hosts are offline. Tests deferred.

- mDNS all-services discovery (2026-09-24): query ten TCP service types with
  an eight-result cap, cancel between queries, and label partial snapshots.

- mDNS Browser CSV export and IPv6 details (2026-09-24): Page Back saves
  discovered services to new files under `/crossink/mdns/`. Device testing pending.

- mDNS Browser in Tools (2026-09-23): choose a TCP service type and inspect
  up to eight discovered names, IPv4 addresses and ports. Device testing pending.

- DNS Lookup in Tools (2026-09-23): connect to Wi-Fi and resolve one hostname
  to an IPv4/IPv6 address. Network/device validation remains pending.

- WiFi Scanner signal history (2026-09-22): selected-BSSID passive sampling,
  last 40 outcomes, min/mean/max and explicit missing/error gaps. Sampling pauses
  outside the signal view. Device validation pending.

- WiFi Scanner CSV export (2026-09-22): Page Back saves the retained snapshot
  to a new numbered file under `/crossink/wifi/`. Earlier exports are preserved;
  SD failure and device validation remain pending.

- WiFi Scanner channel view (2026-09-21): observed AP-count chart and per-channel
  peak/mean RSSI from the capped snapshot. Page or Up/Down switches views;
  Left/Right selects the channel. Device validation pending.

- Recon WiFi Scanner snapshot (2026-09-21): passive scans with up to 40 APs,
  signal-sorted details and manual rescan. Charts/export and device testing pending.

- WiFi Networks in Tools (2026-09-21): scan, select and save networks using
  the existing connection screen; disconnects when leaving. Device testing pending.

- Stego Notes (2026-09-13): select a drawing, compose a bounded encrypted note
  into a new BMP, and unlock notes with timed reveal. Experimental; runtime
  crypto, file failure and device validation remain deferred.

- Authenticator and TOTP QR (2026-09-13): shared encrypted accounts, six-digit
  SHA-1 codes and code-only QR display. System clock sync is available on
  devices without an RTC. Ports remain wip pending consolidated validation.

- Password Manager (2026-09-13): encrypted SD vault, eight entries, create/unlock,
  replace/delete, timed reveal and idle locking. Experimental; use test
  credentials while crypto, recovery and lifecycle validation remain open.

- Habit Tracker (2026-09-12): persistent checks, explicit sessions and
  finished-session streaks. The port remains wip pending V1 validation.

- Flashcards in Tools (2026-09-12): bounded SD decks, flip/grade controls and
  session results. Port remains wip pending consolidated testing.

- Event Logger in Tools (2026-09-10): save and browse short SD notes with
  explicitly labeled uptime timestamps. Port remains wip pending V1 testing.

- Barcode Generator (Code 128B, Code 39, EAN-13) and schematic Key Bitting
  Charts in Tools (2026-09-10). Both remain **wip** pending consolidated tests.

- Etch-A-Sketch with a compact drawing grid and BMP export, plus a Tools
  shortcut to the existing file browser (2026-09-09). The new drawing port
  and launcher route await consolidated testing.

- Sudoku and Maze in Games (2026-09-09), with bounded solver/storage work;
  both remain **wip** pending consolidated gameplay and hardware testing.

- Snake, Minesweeper and Tetris in Games (2026-09-09); implementation ports
  remain **wip** while comprehensive gameplay and hardware testing is deferred.

- A documented partition/recovery proposal, kept separate from the unchanged
  shipping partition table. It is not an activated firmware configuration.

- Simulator button regression checks for the new apps, known-pattern Game of
  Life tests, and an opt-in 50-cycle/ten-minute lifecycle runner with real
  allocator measurements and retained logs. Entry-screen soaks do not prove
  full interactive or hardware correctness.

- QR Generator, Cipher Tools, Clock, OTP Generator and Game of Life are
  available in Tools/Games. These ports remain **wip** pending their documented
  device and resource checks. Cipher is explicitly a toy; simulator Clock/OTP
  show unavailable capabilities instead of fabricated RTC/entropy results.

- A **Tools launcher**, reached from Home. Reading stays the home screen; the
  tile grid (Tools, Games, Recon, Defense, Comms) is one press from it, never in
  front of it. It uses the same list, header and button-hint components as the
  reader, so the launcher and every app inside it share the reader's typography
  and button grammar.
- **Calculator**, **Unit Converter**, **Morse Code**, **Countdown** and
  **Dice Roller** — the first five apps, all offline, all operable on the
  device's buttons.
- Encrypted storage for anything on the SD card worth protecting: AES-256-GCM
  with the key stretched from a passphrase by PBKDF2-HMAC-SHA256, so a stolen SD
  card yields ciphertext rather than credentials. A wrong passphrase or an edited
  file fails cleanly instead of returning plausible-looking rubbish, which is what
  lets a duress PIN show a decoy vault that cannot be told apart from the real one.
- Single-owner arbitration for the radio, so one screen can no longer leave WiFi
  in a state that breaks the next screen's scan or download.
- A fixed-size buffer for raw radio capture that drops and *counts* frames when it
  fills, so a capture screen can report honestly how much it missed rather than
  silently overwriting.
- `docs/merge/PORT_LEDGER.md`: per-app status for the whole ~80-app scope, so
  "what's left" is a file rather than something each session reconstructs.
- `docs/merge/RADIO_MIGRATION.md`: the ordered plan for moving CrossInk's own
  eight network screens behind the new radio arbitration, lowest-risk first, with
  the OTA path explicitly last.

### Changed

- OTA Wi-Fi ownership (2026-09-21): updates now require their own station
  session, and all Wi-Fi picker callers require explicit ownership. Hardware
  update/recovery validation remains pending.

- OPDS and Calibre (2026-09-20) now own their Wi-Fi sessions and check the
  connection before network work. Calibre's parent no longer tears down its
  radio. Runtime and device validation remain deferred.

- Settings Wi-Fi and KOReader account authentication (2026-09-20) now own
  their radio sessions. Settings disconnects when its picker returns; account
  authentication leaves another owner's session untouched. Device checks pending.

- Wi-Fi selection status and event reporting (2026-09-20) now use the radio
  manager, completing removal of direct Wi-Fi SDK calls from the picker.
  Legacy ownership handoff and device validation remain pending.

- Wi-Fi selection connection setup and disconnect (2026-09-20) now check
  radio ownership through the manager. Saved-network policy is preserved;
  device validation remains deferred.

- Wi-Fi selection (2026-09-20) now uses checked scan access and retains at most
  40 scan results to bound the picker list on C3 devices. Manual SSID entry
  remains available. Runtime validation is deferred.

- Wi-Fi selection (2026-09-19) now checks authorization from migrated parent
  screens, preserves their radio hold on return, and cleans up unfinished
  exits. Source integration remains experimental pending device validation.

- Nearby stats and book-position sync (2026-09-19) now reserve their radio
  sessions and clean up failed startup before exit. Device sync and lifecycle
  validation remain deferred.

- Web file transfer (2026-09-17) now owns its AP/station radio sessions and
  reports startup failures. Existing hotspot settings and fast-exit restart
  remain; device/network validation is deferred.

- KOReader sync (2026-09-14) now reserves its radio session, releases it on
  upload/error/exit paths and retains the reader restart flow. Network and
  device validation remains deferred.

- Clock sync and font downloads (2026-09-14) now reserve their radio session
  and release through RadioManager. Busy foreign sessions stay intact. Device
  and network lifecycle validation remains deferred.

- The simulator smoke test now walks the app registry, entering and rendering
  every registered app on each run. Adding an app to the launcher adds it to the
  crash tripwire automatically — nothing to remember, and no harness to update.
- The firmware-size check now warns when free flash drops below a configurable
  reserve, instead of only failing once the image no longer fits. The hard
  failure comes too late to be useful when apps are being added steadily — by
  then the only options are to undo the work or repartition. It warns today, at
  251 KB free against a 256 KB reserve, which is exactly the intent.
- CI now runs the merge's budget gate and native unit tests **in addition to**
  CrossInk's own formatting and static-analysis checks, and builds the simulator
  target alongside the device targets. The required-checks gate covers all five.
- New apps are scaffolded into `src/activities/apps/`, which the firmware
  actually compiles.

### Fixed

- QR/Cipher keyboard activity allocations check failure; Game of Life releases
  partially allocated boards. Shared UI allocation limits remain documented.

- Home menu navigation counts the Tools entry so all menu items remain reachable.
- WiFi scan results use a bounded, terminated SSID copy available on both build
  platforms. The credential-obfuscation comment now correctly states that a
  known device MAC can decode the data on another computer; storage is unchanged.

- Cipher Tools handles very large Caesar shifts without integer overflow and
  clears partial output after malformed hex/Base64 decoding.

- EPUB two-class CSS selectors now match regardless of pair order. Tag-qualified
  compound rules retain precedence over bare compounds; cache version 17
  rebuilds earlier cached styles.

- Unit Converter could show a truncated number as if it were a result. A value
  too wide for its field was cut rather than rounded — "1234567.89" becoming
  "1234567" reads as a plausible answer. It now renders into scratch first and
  publishes only what fits whole, dropping precision (and finally falling back
  to a visible marker) rather than ever showing a fragment. Found by static
  analysis, and there are now tests across every buffer width from 2 to 32 bytes.
- Unit Converter's temperature scale, inherited from the source project, used
  rounded constants (0.5556 / −17.7778). Converting °C to °F and back did not
  return the value you started with. It now uses exact fractions and round-trips
  cleanly; there is a test asserting it across −100 … +100 °C.
- Pound and ounce now use their exact definitions, so 1 lb converts to exactly
  16 oz rather than to five decimal places.
- The app scaffolding template produced code that could not compile: it included
  a header that does not exist and referenced UI strings that were never defined.
  Every app generated from it would have failed on first build.
- CI referenced a build target and a test script that do not exist in this
  repository, so several jobs could only ever fail.
- A unit-test target could not link because its build file omitted one of the
  source files it tests. It had presumably been broken for a while, since no CI
  job ran this suite before; the whole suite is now green (197 tests) and runs
  on every push.

### Attribution

- Ported files now each carry a provenance comment naming their source project,
  licence and copyright holder. `NOTICE` previously claimed every ported file
  "keeps its upstream copyright header", which could not be true: neither
  CrossInk nor biscuit puts a header on individual files. It now describes what
  is actually done — per-file provenance comments, per-app `PORT_NOTES.md`, and
  the MIT notice carried in `LICENSE.merge`.
- `LICENSE.merge` credited "biscuit contributors" separately; both projects are
  MIT with the same copyright holder, and it now says so precisely.

### Documentation corrections

Three v1 documents described work that was not in the tree. Corrected in place
rather than restated, because a merge gate whose own records are aspirational
cannot gate anything:

- `GOVERNANCE.md`'s scope ledger claimed 21 gate-passed apps and three landed
  foundation libraries. The tree contained neither: no `RadioManager`,
  `RingBuffer` or `SecureStore`, and one app parked in a directory the firmware
  does not compile. What v1 did deliver — the ruleset, checklist, port guide,
  budget checker, PR template — is real, and is what this wave builds on.
- `docs/merge/SECURITY.md` reported the credential-encryption, duress-PIN and
  radio-callback findings as fixed and verified. They were not. Each entry now
  states what is in the tree, what is not, and how to re-check it.
- `docs/merge/ACCEPTANCE.md`'s v1 sections test apps that do not exist yet; they
  are now labelled as criteria-for-later rather than a checklist to work through.
- `docs/merge/RUNBOOK.md` described a bootstrap-and-apply-a-kit flow, a QEMU
  smoke script, and a build target — none of which exist in this repository.
  Rewritten against the commands that actually run.
- `AGENTS.md` listed `pio run -e x4-pro` as a validation command. That build
  target does not exist — only `x4-pro-simulator` does — so the X4 Pro is a
  supported device with nothing to flash. The command list is corrected and the
  missing target is recorded in `PORT_LEDGER.md`; the guide also now notes the
  two environment constraints that cost real time to rediscover (no arm64
  cppcheck, and ESP-IDF rejecting paths with spaces).

### Decisions logged

Judgement calls made against `GOVERNANCE.md`'s five-question bar, recorded here
rather than asked, per the v2 brief:

- **The reader's three planned upgrades were already implemented.** The
  single-pass structural cache (including an inflated-HTML cache, so changing
  font or size re-flows without re-reading the EPUB zip), Liang/TeX hyphenation
  across ten languages with pattern tries in flash, and true bold/italic faces
  are all present and tested in CrossInk. They are verified and left alone.
  Rewriting working reader code carries a guaranteed downside and no upside. One
  genuinely open item remains: confirming inline images and justification behave
  identically through the cached path at every font size.
- **The banned XOR credential obfuscation stays, for now, with a stated reason.**
  WiFi and OPDS passwords must be readable unattended at boot, so they cannot be
  keyed on a user passphrase; dropping `SecureStore` in would mean typing a PIN
  before WiFi worked. The options and their trade-offs are written up in
  `SECURITY.md`. Choosing one is a product decision. Until then the wording that
  calls it encryption overclaims, and the doc says so.
- **`SecureStore` is deliberately not compiled into the simulator.** The
  simulator has no mbedtls, and stubbing crypto so a vault app appears to work on
  a desktop is the security theatre the ruleset bans. It is covered instead by
  host tests linking a pinned mbedtls.
- **BLE apps are blocked, not deferred by preference.** This tree has no BLE
  stack at all. Adding one is a real memory cost on the C3 and has to clear the
  gate on its own merits.
- **`SSID Channel` is out of scope**, despite appearing in the Comms tile:
  encoding data into a broadcast SSID means transmitting a crafted beacon, which
  is the offense boundary the brief draws.

### Known constraint — the app scope does not currently fit

Now measured rather than estimated. The launcher plus five apps cost 39,264
bytes of flash: about 18 KB one-time for the launcher, and **~4 KB per app**.
The firmware sits at 95.9% of its 6.4 MB partition with **251,744 bytes free**.

Roughly 68 apps remain in scope. At 4 KB each that is ~272 KB — already over
budget, and 4 KB is the average of the five *simplest* apps on the list. A chess
engine, a packet-capture writer, a BLE stack and a hardware-vendor lookup table
are all far larger.

The likely resolution is a partition change: `partitions.csv` reserves 3.4 MB
for a filesystem partition that nothing in this firmware mounts (settings,
caches and books all live on the SD card), and reclaiming it would cover the
whole scope comfortably. That is a change to the OTA layout, so it is
deliberately **not** bundled into this wave — it needs its own change with the
recovery path tested on real hardware first. Details and alternatives are in
`docs/merge/PORT_LEDGER.md`.

Memory, by contrast, is comfortable: 17.6% of RAM. The budget gate measures RAM
only and would pass a change that cannot be flashed; it needs a flash column.

---

## [v1.5.0] - 2026-08-08

### Added

- EPUB tables now lay out a row at a time in both Incremental and Full Section indexing, keeping regular tables readable without whole-table buffering.
- Touch support for Seeed Studio Sticky
- Nearby File Transfer can send EPUB, TXT, XTC, XTCH, PNG, and BMP files directly between two CrossInk devices without a Wi-Fi network.
- Recent Books and image-file long-press actions can send files directly to a nearby CrossInk device.
- Dictionary lookup and lookup history
- EPUB books can use a dedicated SD-card dictionary font while keeping a different reader font.
- EPUB books can set a dedicated dictionary font size independently of the reader font size.
- Dictionary font and size defaults can be set globally from Settings > Reader > Font Options, with per-book choices still taking precedence.
- Reusable dictionary SD-font builder with IPA coverage and per-family ZIP packaging
- RTC-enabled devices can now choose the date format and numeric separator shown in headers from Settings > System > Device.
- The web EPUB optimizer now splits oversized chapters into memory-friendlier sections before sending them to the reader.
- Reader indexing can now use `Incremental` or `Full Section` mode globally or per book; changing modes keeps the current chapter readable and applies when the next chapter needs indexing.
- Look Up Word can now be assigned to short- and long-press Power button shortcuts.
- EPUB readers can now choose from five word-spacing levels, from normal through extra-wide.
- EPUB inline-image pages on X3 now use the grayscale-aware display base before the image grayscale overlay, reducing the moment where images appear too dark before settling.
- EPUB publisher small-caps styling now renders ASCII lowercase text as smaller capital letters without needing extra font files.
- When incremental EPUB indexing runs out of memory at the first unindexed page, the reader now silently restarts once and resumes the book with a fresh heap.

### Changed

- Reader font sizes now persist as actual point sizes, keeping the closest matching size when font families or installed files change.
- SD-card fonts now include the built-in reader fallback stack for common symbols, emoji, and selected CJK glyphs while retaining Noto Sans fallback coverage.
- Downloadable SD-card fonts are now rendered with the same darker anti-aliasing as the built-in reading fonts.
- Full-section EPUB indexing now prepares one-page chapters and direct jumps to a chapter's last page, while avoiding repeated checks after the next chapter is ready.
- EPUB grayscale rendering now reuses its 8 KB strip buffer across stable pages, reducing repeated heap allocation and release during long reading sessions.
- Reading progress is now saved in batches during ordinary page turns, immediately after layout changes, and when leaving a book, reducing repeated SD-card writes without carrying stale pagination into the next session.
- SD-card font discovery now waits until a custom font is selected or font settings are opened, reducing SD-card work during normal startup with built-in fonts.
- EPUB page turns using SD-card fonts now prepare the next page's glyphs while the reader is idle.
- Dictionary lookups now reuse open index files for stem matching, reducing repeated SD-card work after a miss.
- The web file manager now batches directory listings into fewer network packets, improving large-folder response time.
- Firmware releases now identify the supported device type: X3/X4 or Seeed Sticky.
- Image-heavy EPUB chapters now index by reading image headers first and extract each full image only when its page is shown.
- EPUB books with repeated byte-identical stylesheets now parse each unique stylesheet only once when building caches.
- SD-card fonts now reuse their page-sized glyph buffers, reducing heap fragmentation during long reading sessions.
- Firmware builds now prioritize usable heap over oversized system timer stacks and maximum WiFi throughput, leaving more memory for reading and network operations.
- Downloaded-font size range options now show their actual point-size ranges instead of firmware build names.
- KOReader Sync and authentication, OTA updates, and OPDS browsing now restart into a lightweight network mode that leaves reader and Home data unloaded, providing more contiguous memory for WiFi and secure connections.
- The web file manager can now delete non-empty folders recursively and, when hidden files are shown, remove hidden or system-managed SD card items after confirmation.
- SD-font, OPDS catalogs, and other unneeded settings now stay out of memory while reading unless their settings are open.
- EPUB books can now keep more saved clippings without loading every clipping's text into memory while reading.

### Removed

- The font download manager no longer offers a Download All action; fonts can still be downloaded individually or updated together.

### Fixed

- Book menu tab navigation, popup scrolling, customized Reading Stats hints, and short button presses after low-power mode now work reliably.
- Sleep screens now honor the current orientation, avoid X4 transition flashes, fall back to a valid wallpaper when needed, and handle low-memory image decoding without rebooting.
- Choosing Set Cover uses the selected image in place, and Home no longer repeatedly generates missing EPUB covers.
- Finished-book suggestions are now collected before an EPUB is moved to `/Read`.
- Manage Fonts now opens and scans large catalogs more safely on X3/X4, reports low-memory failures instead of restarting, and returns to Font Options when cancelled.
- Network screens refresh cleanly on X4; long errors wrap correctly; saved Wi-Fi networks and KOReader connections recover more reliably after restart or a missing address.
- Translated Wi-Fi and clock labels no longer truncate text or time values, and clock sync no longer risks a reboot while saving settings on memory-constrained X3/X4 devices.
- KOReader Sync no longer crashes during time setup, re-triggers while connecting, or loses precise EPUB positions; CrossPoint-only data stays on the official CrossPoint Sync server.
- Firmware updates reject images for the wrong chip family, and saved Wi-Fi settings safely handle concurrent access and corrupted values.
- EPUB opening, reflow, and background indexing now handle fragmented memory more safely, retry recoverable work, remain responsive to input and setting changes, and show useful errors instead of rebooting or silently returning Home.
- Low-memory EPUB grayscale and sleep rendering now fall back safely without leaving stale display content.
- Full-section indexing preserves more memory for large chapters and cancels speculative work on page turns, keeping the reader responsive.
- SD-card font and clipping work now release temporary data at the right time, preserving memory for reflow, dictionary use, covers, and thumbnails on X3/X4.
- EPUBs with book-specific built-in fonts no longer load an unnecessary global SD-card font, and custom fonts retain ligatures.
- EPUB styling choices apply before style caches load; CSS-heavy books use less temporary memory; and disabling Embedded Style consistently skips unused stylesheet work.
- EPUB layout now keeps CJK ruby and spaces, Russian paragraph continuations, Bionic Reading, underline/strikethrough runs, and right-to-left text correct.
- EPUBs with flowing `<br>` elements, image-led or decorative chapter headings, unsupported images, and dense final pages now lay out without excess gaps, clipping, dropped images, or misleading low-memory warnings.
- EPUB footnote and cross-reference previews now show complete notes, including targets in the middle of a paragraph.
- Saved EPUB positions, clipping highlights, and selections now stay accurate after font, orientation, or indexing changes; selections also remain readable in dark mode and on memory-tight pages.
- Dictionary misses can switch dictionaries without leaving the reader, and dictionary read failures now report an error instead of a false “not found.”
- Reader popups, KOReader Wi-Fi labels, Lyra battery headers, and the sleep message now remain correctly oriented and positioned.
- Manual refreshes preserve EPUB and TXT text anti-aliasing; XTC and XTCH status bars show the configured time-left estimate.
- Watchdog resets return to the crash-report flow, and power-button wake timing no longer depends on SD-card startup.
- The web file manager and uploads now handle simulator/device ports and stalled connections safely; unsupported settings stay hidden, and the optimizer removes empty chapter stubs without breaking table-of-contents links.

## [v1.4.0.1] - 2026-07-28

### Added

- Updates to support Xteink device detection so the correct display panel driver is used.

## [v1.4.0] - 2026-07-10

### Added

- Dashboard UI theme for the Home screen, showing the current book cover and reading stats.
- Nearby Position Sync for sending or applying the current EPUB position between two CrossInk devices over ESP-NOW.
- Web EPUB optimizer support for CrossInk location metadata, so optimized EPUBs can keep better progress and stable page numbers.
- Reading Stats support for XTC and XTCH books, including reader menus, Home and sleep screen stats, mark finished, delete stats, and preserving stats when clearing book caches.
- Web file manager image previews, so PNG, JPEG, BMP, GIF, and WebP files can be viewed inline before downloading.

### Changed

- Large EPUBs, SD-card font-heavy books, and cover thumbnails now open, index, and generate more reliably under low-memory conditions.
- Home and sleep screens now load more cover and thumbnail data only when needed, reducing reader startup work and reusing cached cover data where possible.
- Built-in reader font choices have been reduced to Lexend Deca and Bitter, reducing firmware size while keeping fallback glyph coverage.

### Removed

- Teensy firmware builds are no longer produced for releases or release candidates.

### Fixed

- EPUB render-mode and Safe Mode toast messages now clear reliably, even when the reader is low on memory.
- EPUB Reading Stats no longer drops unsaved page-turn counts after viewing the stats screen mid-session.
- KOSync is more reliable with many SD-card fonts installed, reducing low-memory failures during secure sync requests and uploads.
- Web file manager actions now handle filenames with special characters safely and reject unsafe rename characters before saving.
- Auto Turn interval settings and related action prompts opened from long-press shortcuts now stay open after releasing the shortcut button.
- EPUB footnote previews no longer show clipped status-bar labels or misleading reader progress indicators, and clipping selection now works from footnote previews.
- Font selection no longer reopens the font preview after choosing a font.
- EPUB chapters with stale publisher style data now rebuild it instead of opening without the book's styling.
- Large SD-card font EPUBs no longer overlap characters after font or line-spacing changes, and clipping selection can fall back to a built-in UI font when needed.
- EPUB cover and thumbnail generation is more reliable with custom SD-card fonts selected and optimized books under low-memory conditions.
- Web EPUB optimizer now preserves more PNG and SVG artwork on-device, including transparent PNGs, dividers, and images in malformed or XML-declared chapters.
- Unsupported SVG images in EPUB chapters are now skipped silently instead of triggering low-memory image warnings.
- Nearby Position Sync now silently restarts back into the reader after using ESP-NOW, matching other WiFi sync flows and reducing post-sync memory fragmentation.
- EPUB page cache loading now uses fewer small heap allocations, reducing fragmentation-related reader failures.
- EPUB grayscale page turns on X3 now use the grayscale-aware display base, reducing the moment where new text appears too dark before the anti-aliased overlay finishes.
- EPUB chapters with many inline anchors, footnote links, malformed XHTML, large publisher styles, or SD-card fonts are less likely to fail or get stuck on the indexing screen.
- EPUB opening and image rendering now recover from more low-memory conditions instead of rebooting, including landscape image pages and books that need lighter render modes.
- EPUB clipping selection now follows right-to-left line order when selecting Hebrew and other RTL text.
- Lyra Carousel no longer shows a blank carousel after returning from WiFi-related File Transfer screens and moving between the menu row and book row.
- Generated SD-card font packages now include the same core glyph coverage as built-in reader fonts.
- Manage Fonts no longer crashes while loading or reloading large SD-card font lists.
- Minimal Home no longer swaps to another recent book when returning from Settings when Back button is mapped to the first button.
- Cancelling a font download now stops on the first Cancel button press instead of needing several presses.
- The `Inverted` sleep cover filter now keeps book covers unchanged on Minimal and Dashboard sleep screens while switching the background to white.
- Rare EPUB open or thumbnail crashes during ZIP decompression are fixed.

## [v1.3.4] - 2026-06-24

### Added

- File Browser now indexes large SD-card folders so directories with many books can be browsed without loading every filename into memory at once.
- EPUB text clipping with saved highlights, clipping lists, and Kindle-style `/My Clippings.txt` export.
- `Create Clipping` is now available as a reader shortcut for short/long Power, long-press Menu, and long-press Back actions.
- Per-book EPUB options for font, layout, styling, reading aids, and render modes, including `CrossInk Default`, `Balanced`, and `Light` modes for difficult books.
- Arena allocator (`lib/Memory/Arena.h`) for burst-then-discard allocation patterns - reduces heap fragmentation during EPUB parsing and page layout over long reading sessions.
- Optimized EPUBs now store location metadata at `META-INF/x-locations.json`.
- X3 SD-card writes now use the RTC for file timestamps when the clock is available.

### Changed

- The EPUB reader menu now splits the growing menu into 3 screens, labels per-book settings as `Book Options`, and avoids showing duplicate `Orientation` controls.
- The `Inverted` sleep cover filter now flips Minimal and Reading Stats sleep screens to black text on a white background.

### Fixed

- Calibre Wireless transfer status no longer stacks the last received-file message on top of the upload percentage.
- X3 Tilt Direction now labels left/right choices as `Left-Right` and `Right-Left`, with existing left/right preferences migrated to keep the same physical tilt behavior.
- EPUB layout now honors publisher page-break CSS, avoids stretching justified spaces before closing punctuation, and keeps large CSS rule sets in a smaller disk-backed lookup cache.
- EPUB first-open conversion now uses more compact OPF manifest lookups and streams cover-wrapper parsing to avoid large temporary heap buffers on books with huge manifests.
- EPUB chapters that run out of memory now retry with `Balanced`, `Light`, and final `Safe Mode` rendering before showing an error, apply the same fallbacks during next-chapter pre-indexing, and let book action menus reset a book's reader settings if Safe Mode still cannot open it.
- EPUB reader font-size changes now restore the current chapter position by content instead of jumping far backward after re-indexing.
- Reading Stats now use the reader's last live book time-left estimate instead of showing a separate fallback estimate.
- Per-book reading stats now migrate compatible legacy `stats.bin` files into the `stats_v5.bin` flow instead of resetting when only the old filename exists.
- Lyra Carousel Home menu rendering now avoids extra label allocations that could crash builds under low memory.
- Lyra Carousel Home cover refresh no longer risks a reboot when memory is tight after returning to or selecting a recent book.
- EPUB image-heavy chapters no longer risk a reboot while saving their reading cache under low memory.
- TXT readers now stay open when pressing a page-turn button at the end of the file.
- Long-press reader shortcuts that open another screen no longer close or confirm it again when releasing the shortcut button.
- RoundedRaff's header battery icon and percentage now sit lower to avoid clipping at the top edge.
- Lyra Carousel now keeps the Home header current when rendering the menu or restoring cached carousel frames, preventing stale battery and clock values while navigating between books.
- Web file manager multi-delete now handles larger selections without failing after a small batch.
- Portuguese EPUBs now use Portuguese hyphenation rules instead of leaving long words unhyphenated when Hyphenation is enabled.
- Progressive JPEG EPUB covers now render more smoothly in generated cover and thumbnail BMP assets.
- EPUB section layout now flushes long text runs earlier when Bionic Reading or Guide Dots are enabled, reducing low-memory failures on difficult books.
- Footnotes in EPUBs with very large shared notes sections no longer cause long stalls when opened.
- Firmware updates now follow GitHub asset redirects before streaming the install.
- Tiled grayscale rendering now serializes display transfers on the shared SPI bus to avoid display glitches during SD activity.

## [v1.3.3] - 2026-06-13

### Added

- `File Browser Display` in `Settings > System > Files & Cache` for choosing one-line or two-line file browser rows across all themes, while preserving Minimal users' existing two-line display on upgrade.
- `Hide File Extension` in `Settings > System > Files & Cache` for expanding file-browser filenames by hiding the right-side extension label.
- Device Name in Settings > System > Device for customizing the KOReader Sync and Nearby Stats Sync device label.
- Additional shortcut options and new ability to add custom shortcuts for Long-press Back Action.
- Delete Reading Stats actions in the EPUB reader and book action menus for clearing one book's stats without deleting its cache.

### Changed

- CrossInk settings now save to `/.crosspoint/crossink-settings.json`, with a one-time fallback migration from `/.crosspoint/settings.json`, so switching between firmware builds is less likely to reset preferences.
- The X3 clock visibility setting is now phrased as `Hide Clock`, with existing `Show Clock` preferences migrated to the matching hide behavior.

### Fixed

- RoundedRaff's date shown in settings now sits lower on X3 devices instead of overlapping the battery.
- Clear Bookmark List now asks for confirmation before deleting a book's bookmarks.
- Clear Reading Cache now preserves per-book reading stats while continuing to leave all-time reading stats untouched.
- Moving finished EPUBs to `/Read` now consistently preserves reading progress, per-book stats, bookmarks, and resume state.
- Book settings option lists now return to the submenu they were opened from when pressing Back.
- Lyra Carousel now refreshes its cached Home icon row after OPDS, Reading Stats, or Bookmarks icons appear or disappear.
- KOReader Sync failure screens now wrap long error messages and shut down WiFi cleanly before returning to the book.
- Sleep Screen > Cover now generates the current book cover on demand instead of falling back to the dark sleep screen when the setting is changed after opening a book.
- File Browser now previews PNG images instead of trying to open them as EPUBs, and hides common macOS and Windows metadata files.
- File Browser now refreshes immediately after falling back to the root folder from a stale saved path.
- File Browser now stops loading oversized folders before low memory can crash the device and shows a memory error instead.
- TXT reader long-press Power page turns now work when Long Power Button is set to Page Turn.
- SD-card font read failures no longer risk a reboot while cleaning up the failed file read.
- Page Overlay sleep screens no longer force EPUB chapters to re-index after waking.
- Page Overlay sleep screens now use the current screen as the overlay background outside the reader instead of trying to rebuild a stale book page.

## [v1.3.2] - 2026-06-10

### Added

- Current date in the top-right Settings header on X3 devices.
- Dark Reader Mode for EPUB and TXT reading screens, plus shortcut actions for the power button and front-button long press.
- File Browser long-press folder action for choosing a custom sleep-image folder instead of only `/.sleep` or `/sleep`.
- Expanded X3 Reading Stats, including streaks, time charts, editable dates, all-time backups, reset controls, an idle-time threshold, and the `Minimal Stats` sleep screen.
- `Reset Reading Pace` in the EPUB reader menu when Time Left is enabled, for clearing only the time-left pace estimate while keeping book reading stats.

### Changed

- Display, Reader, and Controls settings now open list menus instead of cycling through options one by one.
- The X3 clock visibility setting is now phrased as `Hide Clock`, with existing `Show Clock` preferences migrated to the matching hide behavior.
- Reading time and time-left pace tracking now ignore page intervals longer than the configured idle-time threshold.
- Web portal pages now use shared templates, stylesheet, and logo assets, reducing on-device page size and improving browser caching.
- Already-cached EPUBs now open directly to the first page without an extra book-loading popup refresh.
- Reader font-size choices now show point sizes like `10 pt` instead of names like `Tiny`.

### Fixed

- Inverted reader menus now honor orientation-aware side-button navigation.
- EPUB book time-left estimates now wait for more session pace samples and use a progress-based floor after pace data exists, reducing swings from unusually short or long pages.
- Deleting an EPUB book cache now preserves that book's reading stats and pace data.
- X3 clock settings now have clearer UTC offset editing, and `Sync Date/Time` can use saved WiFi networks automatically.
- Home, Lyra Carousel, WiFi setup, and SD-card font flows now release memory more aggressively to avoid freezes or crashes on constrained builds.
- Vietnamese settings labels no longer show replacement diamonds after generated translation offsets shifted.
- KOReader Sync now lands correctly at chapter starts and shows more specific connection guidance.
- EPUB bookmarks saved under the old unstable path hash now show up again, including for books moved to `/Read`.
- SD-card font downloads now use versioned direct S3-hosted HTTP endpoints with CRC validation, avoiding GitHub release redirects and ESP32-C3 TLS stalls when loading the font catalog.
- EPUB text blocks now keep the book's alignment style when an inline image appears before the text.

## [v1.3.1] - 2026-05-28

### Added

- EPUB reading-position improvements, including bookmark anchors, bookmark preview snippets, and optional chapter/book time-left estimates.
- Nearby Reading Stats sync with separate totals for this device and all synced CrossInk readers.
- Per-server OPDS filename settings so downloaded books can use either Author - Title or Title - Author.
- EPUB render heap diagnostics that include the largest allocatable block, not just total free heap.

### Changed

- Moved the X3 reader clock into a new top-centered status bar and moved clock settings to Settings > System > Device.
- Reworked Display, Reader, Controls, in-reader options, and larger System settings groups so related options open as submenus.
- Improved OPDS and font download responsiveness by reducing progress-update overhead and temporarily disabling WiFi power saving during transfers.
- Book selection now shows a loading popup before EPUB indexing or cache loading begins.
- Delayed the automatic finished-book prompt until the reader leaves the chapter where they reach 99%.

### Fixed

- WiFi settings screen now keeps the displayed MAC address consistent with the router-visible WiFi address.
- Reader UI issues with inverted menu button hints, Lyra Carousel popups, and Auto Page Turn interval persistence.
- Web uploads and KOReader Sync progress saves now preserve progress, stats, settings, and valid resume data for refreshed book files.
- OPDS low-memory handling now shows a specific parser-buffer memory message and releases SD-card fonts before catalog loading.
- EPUB cache, CSS, table, SD-card font, and allocation failure paths now recover, retry, or stop cleanly under low memory instead of opening unstyled pages, failing unnecessarily, or risking a reboot.
- EPUB text with invisible word-joiner characters no longer shows replacement diamonds for missing font glyphs.
- Clarified the low-memory EPUB image warning so it says some or all images may be missing.

## [v1.3.0] - 2026-05-21

### Added

- Back/Cancel support while downloading books from OPDS catalogs.
- Recent Books long-press menu in both List and Grid views with delete, cache delete, completion, and remove-from-recents actions.
- Minimal sleep screen option that shows the current book cover and reading progress on a dark background.
- More detailed WiFi connection debug logs for scans, selected networks, status changes, disconnect reasons, and timeouts.
- 9pt `Itty Bitty` reader font size, plus build flags for omitting Itty Bitty and Large reader font assets in size-constrained firmware variants.
- In-reader confirmation message when a shortcut turns tilt-to-turn on or off.

### Fixed

- WiFi and OPDS connection-flow edge cases: manual Settings connections now show the connected status before continuing, copied or corrupted saved-password files are rejected before use, OPDS retries show loading before requests, and large OPDS feeds fail safely under low memory instead of rebooting.
- Reader and Home UI polish issues, including landscape status-bar settings, missing Vietnamese labels, File Browser and Lyra Carousel icon alignment, cover thumbnail artifacts, and duplicate Home progress/stat loading.
- EPUB cache and low-memory handling now use stable cache folder keys, migrate older cache folders where possible, rebuild stale section caches, lay out very long text blocks earlier, stream table fallback content when heap is tight, and clarify the warning text.
- Sleep-entry, network, and SD-card font download reliability improvements: cached sleep-screen assets are reused, OPDS pages idle normally after load, the X3 tilt sensor sleeps outside the reader, WiFi power saving is disabled during transfers, WebDAV stack usage is lower, longer stalls are tolerated, interrupted font files are retried, and active reader fonts are freed when needed.
- Remaining reader service edge cases, including an XTC chapter selector crash on memory-constrained builds, SD-card font size selection, SD-card font-size shortcuts skipping manually installed sizes, and KOReader Sync login compatibility with self-hosted servers that return valid JSON on success.

### Changed

- Modified upstream "page-as-sleep" behavior into a new `Sleep Screen > Quick Resume` option, which also keeps `Quick Resume on Timeout` on, and renamed the timeout-only toggle.
- Improved reader and browser menu behavior by moving the Footnotes shortcut above Select Chapter, wrapping long book titles in action menus, and reducing progress-screen repaint work during OPDS and SD font downloads.

## [v1.2.11.1] - 2026-05-15

### Changed

- Removed Medium font size from `xlarge` build to get it below the size limit

### Fixed

- Lyra Carousel is now included by activating the build flag `DCROSSINK_ENABLE_LYRA_CAROUSEL=1`

---

## [v1.2.11] - 2026-05-14

### Added

- New personal theme: "Minimal"
- Custom sleep timer picker so `Time to Sleep` can be set from 1 to 30 minutes instead of cycling fixed presets.
- In-reader Controls shortcut for customizing buttons without leaving the book.
- Bookmark cleanup shortcuts: hold Select on a bookmark to delete it, or hold Open on a book in Bookmarks to clear that book's bookmark list.
- Confirmation message after deleting a book's cache from the reader or File Browser.
- File Browser long-press action for deleting an EPUB or XTC book's cache.
- Downloaded-font size range setting so SD-card fonts can use compact, default, or large point-size sets.
- File Browser long-press action for marking EPUB books as finished or unfinished.

### Changed

- Hardened deep sleep entry by shutting WiFi down before waiting for the power button to be released.
- Raised the web file-transfer filename limit from 100 to 150 bytes so longer uploaded filenames are preserved.
- Made the in-reader Reader Options menu include the same Reader settings and actions as Settings > Reader.
- Split SD-card font descriptions and supported languages into separate lines in the font download screen.

### Fixed

- Inline EPUB images no longer disappear in landscape when their bottom edge slightly overlaps the screen margin.
- Reduced unnecessary low-memory image suppression for JPEG-heavy EPUB chapters and added CSS heap diagnostics during chapter rebuilds.
- Allowed wider inline JPEG images in EPUBs to render when they still fit the total pixel and heap safety limits.
- SD-card font picker no longer reopens immediately after selecting a font from Settings > Reader > Font Family.
- In-reader font-size changes now work for SD-card fonts.
- In-reader SD-card font changes now rebuild the current EPUB page layout consistently.

## [v1.2.10] - 2026-05-11

### Added

- `Recent Books View` setting so the dedicated Recent Books screen can switch between the classic list and a 3x3 cover grid.
- More flexible reader controls, including orientation-aware front/side button settings, nav-only or all-button front inversion, tilt page turn shortcuts, and side-button long-press rotation actions.
- Per-session auto page turn interval picker with values from 5 to 120 seconds.
- File Browser Home/Back long-press action for toggling hidden files and folders.
- EPUB rendering and diagnostics improvements, including visible `<hr>` separators and heap logs around section rebuilds, image extraction, page serialization, and sleep-cache rebuilds.
- Reader font coverage for block redactions, black-square ornaments, Greek category letters, and turned-comma punctuation (PR #104).
- Simulator tools for testing sleep/wake behavior and smoke-testing common screens and EPUB reader menus.

### Changed

- Reduced Controls settings section spacing so the grouped controls fit better on X3 screens.
- Made front reader long-press actions trigger when the hold delay is reached while normal page turns still trigger on release.
- Used the fast EPUB spine/TOC indexing path for books with 300+ spine entries so heavily split books build `book.bin` faster on first open.
- Allowed the web file manager and WebDAV to browse dot-prefixed hidden files when hidden files are enabled, matching the device file browser.

### Fixed

- Reader button and shortcut behavior, including X3 power-button wake filtering, folder delete long-press timing, and WiFi scan/connect screens that could not be exited while work was in progress.
- RoundedRaff home-menu, keyboard, and button-hint rendering issues so Settings remains reachable and compact labels no longer overlap or disappear.
- Font and glyph handling now reduces persistent SD-card font advance-cache memory, releases optional font caches before image extraction only when heap is tight, and shows a visible replacement symbol when compact UI fonts lack `U+FFFD`.
- KOReader Sync authentication diagnostics and an in-reader sync crash, including clearer handling when a server or proxy returns non-JSON content.
- EPUB text rendering for redactions, whitespace-only XHTML text nodes, simple black CSS span backgrounds, list bullets in `<li><p>...</p></li>` items, and very long base64-like text runs.
- EPUB image, thumbnail, and section-rebuild stability so image-heavy chapters use less temporary memory, scale images more reliably, avoid stale dimensions, and suppress optional image work earlier under heap pressure.
- EPUB low-memory and cache safety now skips optional next-chapter indexing and sleep-page cache rebuilds when heap is tight, fails safely with a malformed-book warning and Home exit path, rebuilds incompatible fork-written caches, and handles low-memory CSS parsing, truncated SD writes, invalid serialized strings, and failed temp-cache promotion.
- Home no longer crashes after clearing reading cache when the source EPUB cache is missing.
- Reader prewarm behavior now skips image decoding, keeps mixed-style font glyphs cached together, and avoids section rebuilds for render-quality-only option changes.
- Concurrent render/storage crashes are avoided by serializing `GfxRenderer` scratch-buffer access, shared SPI bus access, and failed SPI lock cleanup.
- Recent Books, EPUB/XTC thumbnail caches, deleted-folder metadata, and XTC cover scaling now keep cached book data in sync and grid covers fill their slots correctly.
- Simulator build configuration now lets SDL2 and simulator-provided network/OTA shims compile cleanly.

---

## [v1.2.9.1] - 2026-05-03

### Changed

- Cleaned up EPUB table rendering by removing synthetic row/cell labels and defaulting table cells to readable left alignment
- Allow simple EPUB tables with full-width note rows so a single `colspan` cell spanning the whole table no longer forces the entire table back to paragraph fallback

### Fixed

- Power-button shortcut conflicts outside the reader so reader-only actions fall back to `Confirm` while Sleep, Refresh, Screenshot, Sync Progress, and File Transfer remain real power actions.
- Potential crash when using `Go to %` in EPUBs.
- Potential crash when entering sleep with Page Overlay enabled if the cached EPUB page data is invalid.
