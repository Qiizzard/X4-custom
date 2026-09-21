# RADIO MIGRATION — getting every radio user behind `RADIO`

`RULESET.md` rule 7 says all WiFi/BLE goes through `RadioManager`. As of the
foundations landing, **new merge apps do; CrossInk's own network screens do
not.** This file is the standing record of that gap, so it stays a tracked
debt instead of quietly becoming the norm.

## Active P5 implementation (2026-09-14)

The user-approved active queue supersedes the historical one-site/no-batching
cadence below: related source migrations share one compile check, with runtime
and hardware checks deferred to V1. ClockSync and FontDownload now acquire a
named station hold before opening their existing WiFi picker, and release only
their own hold at exit. RadioManager's owner-checked shutdown cannot release a
different owner; station-status reads also require the matching owner. No new
radio mode or dynamic RadioManager storage was added. Picker allocations now
fail cleanly. Font-change restart happens after releasing the owned session.

These two activities no longer directly start/stop/query Arduino WiFi. Busy
managed or foreign legacy sessions are left intact and reported unavailable;
ClockSync no longer silently borrows an already-connected foreign session.
The normal radio-off → picker → operation → exit path remains the intended
flow. Picker success leaves the connection for its parent; cancellation can
stop the driver, followed by the parent's manager cleanup. Startup failures
cannot trigger font-network retries without a hold. Simulator refuses radio.

Status: sites 1–8 source integrated, **wip/unverified**; runtime/OTA recovery gates remain open.
The shared WifiSelection child now uses manager APIs and mandatory parent
tokens. Hardware verification remains deferred; source completion does not
establish that every network operation in the wider repository is managed. Picker and OTA source migrations are recorded in the checkpoints below.
No OTA/recovery code or shipping partitions changed.

V1/device checks: on X3/X4, Settings > System > Device > Clock sync, connect,
sync, Back; then enter font downloads and cancel/complete a download, exit, and
open another network screen. Expect clock_sync/font_download acquisition and
release logs, no hold after exit, and normal font-change restart. Cover wrong
credentials, dropped connection, picker cancel, allocation failure, global Home,
foreign-radio denial and attempted release by a different owner. Check C3 free
heap, largest block and task stack high-water marks. No cache reset is required
for radio ownership. No physical or runtime checks have been performed here.

Compile evidence: C3 default PASS (145.447s), `/tmp/x4-p5a-build.log`,
2026-09-14. Firmware 6,391,744 bytes, stock OTA free 161,856 bytes, reserve
warning remains. No runtime/network/host/simulator/hardware tests performed.

## KOReader sync checkpoint (2026-09-14)

KOReaderSync now acquires `koreader_sync` after position/credential checks and
before its existing WiFi picker. Picker allocation is fallible; busy foreign
sessions fail without being borrowed or torn down. The picker still uses SDK
calls inside the parent's hold until site 7 is migrated. Sync/upload entry
checks ownership and a connected station with a nonzero IP; RadioManager also
uses that readiness check for clock/font consumers (matching the prior KOReader
WiFi helper's DHCP check). Simulator remains unavailable, not simulated success.

Render-wait failures, completed upload (including HTTP failure), picker
allocation failure and exit release only this owner's hold. The separate
successful-session flag survives early release so the existing silent reader
restart still happens on exit. Failed acquire does not trigger that restart.
Release refusal logs and prevents exit-triggered reboot of another owner.
The pre-network reader restart, EPUB release/reload timing, NTP fallback,
TLS/credentials, document matching, progress mapping and upload payloads were
not changed. One scalar ownership flag and a static owner label were added;
no new buffers, per-loop allocations or unmeasured memory savings are claimed.

V1/device: on X3/X4, open an EPUB and invoke KOReader sync, select WiFi, fetch
progress and test Apply, Upload, already-synced and no-remote-progress branches.
Confirm progress survives the return-to-reader restart, logs show one ownership
release even after early upload teardown, and the next radio screen works.
Cover picker cancel, missing credentials, OOM, DHCP/disconnect, HTTP/render-wait
failure, global Home and foreign-owner denial. Check heap/largest block and
stack high-water marks across repeated cycles. Cache reset is not required by
this change; an existing missing position-map error still requires the normal
EPUB optimization flow. These are pending checks, not passed results.

KOReader compile evidence: C3 default PASS (31.696s), `/tmp/x4-p5b-build.log`,
2026-09-14. Image 6,392,080 bytes, stock OTA free 161,520 bytes; reserve warning
remains. No runtime/network/host/simulator/soak/hardware tests performed.

## AP manager checkpoint (2026-09-16, unverified)

RadioManager now models WifiAccessPoint and exposes a dedicated configured
acquisition API; generic acquire refuses AP so callers cannot start it without
configuration. SSID is bounded to 32 bytes, password to 8–63 printable ASCII
bytes (nullptr explicitly requests open), channel to 1–13 and clients to 1–4,
matching the live Arduino WiFiAP API. Invalid non-null passwords are rejected,
never downgraded to open. A managed/foreign radio prevents startup. Credentials
are not retained or logged. Failure cleans up the attempted driver startup;
owned shutdown now explicitly disconnects AP before turning WiFi off. Address
lookup requires the same static owner pointer and zeros output on failure.
Simulator startup fails without pretending that an AP exists. No new manager
heap allocations or buffers were introduced; SDK allocations remain unmeasured.

This is a source dependency checkpoint: CrossPointWebServer still uses its
legacy WiFi calls. Next: wire AP/STA selection, address/status display, DNS/web
service cleanup and the restart-before-socket-close path to owned sessions.
Do not mark site 4 migrated or claim a working AP through RadioManager yet.
V1/device checks must cover protected/open AP startup, invalid configuration,
busy/foreign ownership, wrong-owner address/release, startup failure and teardown,
phone connection/DHCP, web transfers, mode switching and the next radio activity.
On X3/X4 use the file-transfer AP route after integration; verify existing SSID,
password and client limit, then exit and open clock sync. No cache reset needed.
No runtime/radio/host/simulator/hardware checks have been performed.

AP checkpoint compile: C3 default PASS (220.334s), `/tmp/x4-p5c-build.log`,
2026-09-16. Firmware 6,392,272 bytes; stock OTA free 161,328 bytes, reserve
warning remains. Unused acquisition/address functions may be discarded until
consumer integration; compilation is not runtime AP evidence.

## Web-server AP/STA integration (2026-09-17, wip)

CrossPointWebServer now acquires `web_server` station ownership before its
existing WiFi picker, or configured AP ownership before hotspot services.
Picker cancellation releases the parent hold before returning to mode choice.
AP address and station readiness/RSSI are read through RadioManager. Current
open SSID/channel/four-client settings are unchanged; no password downgrade
fallback remains. Picker, DNS and web-server allocations fail cleanly; DNS is
owned by a unique_ptr and start failures are checked. These replace existing
allocations, not additional frame/loop buffers. Peak RAM is not yet measured.

Normal owned exits retain restart-before-socket-close to avoid the existing
stalled-browser close problem. If restart returns on the deep-sleep path,
services stop before the owned radio is released. Failed AP/STA acquire does
not stop global mDNS/DNS or reboot another radio owner. Busy/startup error has
a translated screen and Back exit. Calibre remains a separate legacy child;
its existing parent fallback restart/disconnect branch is explicitly retained.
Do not claim Calibre, the shared WiFi picker, or every radio path is migrated.
Future picker ownership must support this parent hold rather than steal it.

V1/device: X3/X4 file transfer > Create Hotspot, join from a phone/computer,
check DHCP/captive DNS/address/SSID and upload/download, then exit with idle
and stalled browser connections. Repeat Join Network, picker cancel then AP,
WiFi loss/recovery, DNS/OOM failure, busy-owner denial and reader return path.
Verify subsequent clock/nearby screens acquire normally; exercise deep-sleep
cleanup and the unchanged Calibre branch. Check free heap/largest block and
stack high-water marks. No cache reset is needed. No live network, simulator,
host, soak or hardware validation has been run for this integration.

Web-server integration compile: C3 default PASS (28.283s),
`/tmp/x4-p5d-build.log`, 2026-09-17. Image 6,393,760 bytes; stock OTA free
159,840 bytes, reserve warning remains. All runtime/device checks deferred.

## Why it was not done in one pass

CrossInk predates this arbiter. Eight activities drive Arduino WiFi and ESP-NOW
directly, and they include the paths a bricked device is recovered through (OTA)
and the ones people use daily (OPDS, file transfer, nearby sync). Rewriting all
of them in the same change that *introduces* the arbiter would put the reader's
most load-bearing network paths through an untested layer, in a diff too large
to review honestly. `RULESET.md`'s own tie-breaker applies: stability beats
features.

## What protects the gap in the meantime

`RadioManager::acquire()` calls `foreignRadioActive()` — `WiFi.getMode() !=
WIFI_MODE_NULL` — and **refuses** rather than taking the antenna from a legacy
screen that is mid-download. So the failure mode is "the scanner says the radio
is busy", not "the OTA silently died". That is the right way round, but it is a
guard, not the fix: the legacy screens still leak state between themselves,
which is the class rule 7 exists to close.

## The call sites, in migration order

Safest first. Each row is one reviewed change with its own hardware check — not
a batch.

| # | Call site | Radio use | Mode | Risk |
|---|---|---|---|---|
| 1 | `settings/ClockSyncActivity.cpp` | NTP fetch | `WifiStation` | Low — short, self-contained, easy to re-test. |
| 2 | `settings/FontDownloadActivity.cpp` | HTTPS download | `WifiStation` | Low. |
| 3 | `reader/KOReaderSyncActivity.cpp` | Sync POST | `WifiStation` | Low. |
| 4 | `network/CrossPointWebServerActivity.cpp` | SoftAP + HTTP server | `WifiStation` (+AP mode: needs a new `RadioManager::Mode`) | Medium — AP mode is not modelled yet. |
| 5 | `network/NearbyStatsSyncActivity.cpp` | ESP-NOW | `EspNow` | Medium — verify against the existing `esp_now_init`/`deinit` pairing. |
| 6 | `reader/NearbyBookPositionSyncActivity.cpp` | ESP-NOW | `EspNow` | Medium — same, and it runs inside the reader. |
| 7 | `network/WifiSelectionActivity.cpp` | scan + associate + credential store | `WifiScan` → `WifiStation` | High — 18 call sites; it is the screen every other network feature depends on. |
| 8 | `settings/OtaUpdateActivity.cpp` | firmware download | `WifiStation` | **Highest — rule 23.** Do this one last and only with a tested stock-`update.bin` rollback to hand. |

## Rules for each migration

1. One call site per change. No batching, whatever the temptation.
2. `acquire()` in `onEnter()`, `shutdown()` in `onExit()`, unconditionally — rule 8.
3. `RadioManager` gains a mode only when a call site genuinely needs one (SoftAP
   at #4). Do not speculatively widen the enum.
4. Hardware check per row, named in the PR: the screen still works, **and** the
   next radio screen entered afterwards still works. The second half is the
   whole point.
5. #8 is not done until an OTA has been run and rolled back on a real device.

## Definition of done for this file

When all eight rows are migrated, `foreignRadioActive()` should never return
true. At that point make it an assertion in debug builds and delete this file.

## P5 nearby ESP-NOW ownership (2026-09-19)

Both nearby-sync activities now reserve a named EspNow hold. RadioManager
configures their existing channel and disabled power saving with checked SDK
results; activities retain ESP-NOW protocol, peer and callback management.
Partial startup failure clears callbacks/deinitializes a started ESP-NOW stack
before releasing the hold. Denied acquisition does not switch off another
session. Sync retries after startup failure report unavailable instead of
sending. Book-position sync retains restart-to-reader after radio activation;
denied acquisition does not mark the radio activated. No new heap allocations.

C3 default compile PASS (28.639s), `/tmp/x4-p5e-build.log`; 6,394,528-byte
image, 159,072 bytes free in unchanged stock slots. Source **wip/unverified**.
V1/device: on two X3/X4 readers, exchange nearby reading stats, then share and
apply a book position; test cancel, global Home, startup failure and busy radio.
Expect nearby_stats/nearby_position acquisition/release logs and book-reader
restart after an acquired session. Open clock sync afterward to check reuse.
Check callback teardown, heap and stack watermarks; no cache reset required.
No two-device, simulator, host, soak or hardware verification run in this batch.

## P5 shared picker authorization checkpoint (2026-09-19)

ClockSync, FontDownload, KOReaderSync and web-server STA now pass their static
owner token explicitly to WifiSelection. The picker checks matching station
ownership before entry, scanning, connecting and each loop. Legacy callers may
use the existing path only when no manager hold exists. Denied/lost ownership
shows the translated unavailable error; Back/Done exits without radio cleanup.
Successful completion keeps the parent's connection and hold; cancellation
stops the connection but leaves the manager reservation for parent cleanup.
Unfinished exits default to cancellation cleanup, including global Home.
No new heap allocation or changes to credential persistence/association policy.

This is a **wip/unverified checkpoint**, not completion of site 7. SDK scanning,
association and event logging still reside in the picker. Next: migrate these
operations behind manager APIs and define handoff for remaining legacy parents
(Settings, OPDS, Calibre, KOReader auth and OTA) without orphaned holds. OTA
release/recovery gates remain open. No hardware or deferred V1 tests performed.
Device checks: connect/cancel through each of the four managed parents, cancel
from password entry or global Home, and verify the next radio screen works.
With a different owner held, picker entry must fail without stopping that hold.
Check auto-connect, hidden SSIDs, save/forget and wrong passwords on X3/X4.

Compile evidence: final C3 default PASS (25.824s),
`/tmp/x4-p5f-final-build.log`; 6,394,976-byte image, stock OTA free 158,624
bytes, reserve warning remains. Initial compile passed before the Done-button
correction; final incremental compile checked the corrected source.

## P5 picker asynchronous scanning checkpoint (2026-09-20)

Picker scan start, polling, result extraction and result cleanup now go through
RadioManager. Each operation checks the parent's station token. A null token
is a temporary compatibility path for legacy parents, allowed only with no
manager hold; it does not acquire or transfer ownership. Simulator scan calls
fail instead of inventing radio results. Association/status/event SDK calls and
legacy parent handoff remain next; site 7 is still **wip/unverified**.

The picker retains the first 40 SDK results before deduplication, then sorts
saved networks first as before. It reserves room for these plus the manual
hidden-network action, bounding its vector growth; SDK internal scan memory is
not bounded by this change. Dense scans can omit later networks (manual SSID
entry remains available). One small stack result replaces direct SDK getters;
no extra heap buffer was added. Startup failure follows the existing empty-list
fallback. Credential persistence and association policy are unchanged.

V1/device: scan/cancel/re-scan through clock sync and a legacy Settings caller;
cover >40 results, duplicate/hidden SSIDs, saved-network ordering, failed scan,
wrong-owner denial and another network screen after exit. Check C3 heap/largest
block and SDK scan-buffer release. No cache reset needed. Runtime/device,
host/simulator suites and soaks remain deferred.

Compile evidence: C3 default PASS (32.470s), `/tmp/x4-p5g-build.log`;
6,395,600-byte image, stock OTA free 158,000 bytes; reserve warning remains.

## P5 connection setup/disconnect checkpoint (2026-09-20)

Picker radio mutations now use checked manager APIs: prepare, begin, timeout,
retry, cancel and exit disconnect. The null-owner legacy compatibility path
still requires no managed hold. Preserves SDK persistence disabled, non-erasing
1000ms pre-connect disconnect, all-channel/strongest-AP association and existing
hostname. A 6-byte MAC plus 40-byte hostname buffer replaces transient Strings;
no credentials retained by the manager. Invalid lengths fail without logging
secrets. Simulator connection preparation fails. Status/event reads and legacy
parent ownership handoff remain; site 7 remains **wip/unverified**.

C3 compile PASS (27.781s), `/tmp/x4-p5h-build.log`: image 6,396,144 bytes,
157,456 bytes free in unchanged OTA slots, below reserve. V1/device: connect to
open/protected/hidden and multi-AP networks; test wrong password, timeout,
auto-connect fallback, cancel/global Home and the next radio screen. Verify
router hostname and saved credentials survive; check wrong-owner refusal.
No host/simulator suites, soak or device tests run; no cache reset needed.

## P5 picker status/events checkpoint (2026-09-20)

The picker no longer calls Arduino WiFi or ESP SDK APIs directly. Manager
status snapshots expose connection/failure flags, IP, RSSI, BSSID and channel;
MAC reads also live there. Existing association-complete semantics, retry,
credential save and RTC behavior remain. Simulator returns unavailable, not a
fabricated connection. Legacy parent handoff remains: null-owner compatibility
still only works when no manager hold exists. Site 7 remains **wip/unverified**.

Event logging uses one process-lifetime SDK registration in place of four,
with no activity pointer captured. The SDK owns that existing type of callback
allocation; no per-attempt registration or new result heap buffer. Active/reason
fields shared with the event task are atomic; shutdown disables logging.
Snapshots use small stack storage. No runtime memory/performance claim.
V1/device: connect/cancel/retry via Settings and managed clock sync, check IP,
MAC, signal and disconnect logs, then use another radio screen. Cover dropped
AP, wrong credentials, denied ownership and repeated entry/exit. No cache reset
needed; all host/simulator/soak/device tests remain deferred.

Compile evidence: C3 default PASS (27.492s), `/tmp/x4-p5i-build.log`;
6,396,768-byte image, stock OTA free 156,832 bytes, reserve warning remains.

## P5 Settings and KOReader-auth parents (2026-09-20)

Settings' standalone Wi-Fi picker now holds settings_wifi through the child,
releases on return (including successful selection) and on global exit, and
uses the existing unavailable screen on denial. Saved credentials remain;
Settings no longer leaves an idle connected radio for later screens to borrow.
KOReader authentication/sign-up acquires koreader_auth, requires its connected
station before requests, and releases before the minimal-boot return restart.
Denied ownership cannot borrow, shut down or restart another radio session.
Both picker allocations are fallible. Authentication/TLS/payload semantics are
unchanged. Remaining legacy picker callers: OPDS, Calibre and OTA.

Source **wip/unverified**. V1/device: Settings > Network, save/select/cancel,
return and then run clock sync; expect settings_wifi release at return. In
KOReader settings test authentication/sign-up with test accounts, wrong
credentials, cancel and global Home; expect release and normal full-app
restart after own-session exit. Cover busy foreign radio and allocation failure;
no cache reset. Host/simulator/soak/device tests remain deferred.

Compile evidence: C3 default PASS (37.353s), `/tmp/x4-p5j-build.log`;
image 6,397,296 bytes, stock OTA free 156,304 bytes; reserve warning remains.

## P5 OPDS/Calibre parent batch (2026-09-20)

OPDS and Calibre acquire named station holds and pass them to fallible picker
allocations. OPDS retries reuse its hold; real feed/download operations require
its connected station, while existing simulator fixtures remain unchanged.
OPDS releases before its minimal-boot restart and refuses foreign-session
borrowing/restart. Calibre checks the station before server startup, uses a
fallible server allocation and preserves fast restart before socket teardown;
if restart returns during deep sleep, sockets stop before owner-checked release.
Denied acquisition cannot trigger its global mDNS/restart path. Web transfer's
parent no longer performs the old fallback Calibre radio teardown.

Source **wip/unverified**. OTA is the remaining legacy picker caller; migrate it
then remove the null-owner compatibility path. V1/device: browse/search/download
OPDS, cancel/retry/drop Wi-Fi; connect Calibre and send a test book, cancel/global
Home, return to reader and enter another network screen. Expect named holds,
normal restart and no disruption when acquisition is denied. Check mDNS/socket
cleanup and C3 heap. No cache reset unless separately testing EPUB cache changes.
Host/simulator/soak/device verification remains deferred.

Compile evidence: C3 default PASS (23.453s), `/tmp/x4-p5k-build.log`;
6,397,440-byte image, stock OTA free 156,160 bytes; reserve warning remains.

## P5 OTA and mandatory picker ownership (2026-09-21)

OTA acquires ota_update before a fallible picker launch. Update check/install
entry points require its connected station; denied acquisition cannot borrow
or tear down another session. Exit releases its hold before the existing
back-out restart. Updater hash/device validation, partitions, installation
logic and successful plain reboot path are unchanged. No update was executed.

All nine picker call sites now pass a required explicit token. Manager picker
operations reject null tokens; the legacy startup/shutdown bypass is removed.
All eight planned sites are source-integrated, **wip/unverified**. Retain this
record and foreign-radio detection: compile success is not proof that all
other radio users in the tree are managed. P6 source work can proceed under
the active queue; OTA/recovery gates remain release gates.

V1/device: check for updates, cancel, deny ownership, drop connection before
check/install and verify the next radio screen works. Actual update, wrong
image/hash rejection and known-good rollback require controlled device/recovery
validation later. No flashing, partition changes, host/simulator suites, soaks
or hardware checks performed. No cache reset required for this ownership change.

Compile evidence: C3 default PASS (29.433s), `/tmp/x4-p5l-build.log`;
6,397,552-byte image, stock OTA free 156,048 bytes; reserve warning remains.
