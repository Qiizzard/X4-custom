# X4 merge session report — updated 2026-09-09 UTC

The source integration is pushed to GitHub. The larger project is unfinished:
9 done, 5 wip, 60 todo, 11 base and 4 blocked ledger rows, including foundations
and reader items. Existing done statuses are historical, not fresh hardware
certification. No device was flashed or hardware check claimed in these runs.

## Current evidence

- Batch 1: five 50-cycle/ten-minute entry-screen soaks passed within the
  4096-byte tolerance. Interrupted earlier runs were not passes.
- Batches 2–4: QR/Cipher button navigation and Game of Life known-pattern,
  wraparound and counter tests passed. RTC/OTP capabilities remain unavailable
  in the standard simulator; no physical entropy/time result is claimed.
- Batch 5: compound CSS precedence corrected with a failing-then-passing
  regression; cache version 17, cold/cached rule assertions and stale-version
  rejection passed. Physical EPUB image/layout equivalence remains open.
- Batch 6: existing full-image gate documented; five boundary subcases pass.
- Batch 7: 246 host tests, 16 Cipher UBSan tests, full simulator smoke and C3
  build passed. Caesar overflow and partial malformed-decode output were fixed.
  Source commits: 99ca9764, 3c70ae7a, 8994be8e; evidence commit 349d3d26.
- Latest tested image: 6,316,352 bytes, leaving 237,248 bytes in the 6,553,600-byte
  OTA slot. Low-headroom warning remains. Static analysis is deferred to CI.

See verification/BATCH_1 through BATCH_7 records for exact scope, hashes and
logs. Source code is committed; all later delivery edits are documentation.
The test binary uses the unchanged shipping partitions and is for X3/X4 C3,
not X4 Pro or Sticky. Firmware image checksum and appended SHA validation
passed esptool image-info before delivery.

## Remaining hardware-only work

Use the real X4; keep stock recovery available. No app check below needs an
EPUB cache reset unless explicitly stated.

| Item | Procedure and expected result | Defining document |
|---|---|---|
| Clock | Home → Tools launcher → Tools category → Clock, on a device with a populated RTC. Confirm time advances, synced date/offset/format are correct; verify unavailable-RTC fallback separately. | Clock PORT_NOTES |
| Game of Life | Home → Tools → Games → Game of Life. Confirm advances exactly one generation; Up restarts; counters match cells; Back exits. Check a known oscillator and wraparound; heap returns after exit. | Game of Life PORT_NOTES |
| QR | Home → Tools → Tools → QR Generator. Phone-scan generated text and edited output; initial cancellation also passes in simulator, alongside entry/output/edit-cancel/exit. Measure heap including keyboard and QR generation. | QR PORT_NOTES |
| Cipher | Home → Tools → Tools → Cipher Tools. Exercise every algorithm, key entry, result, retry and cancel; confirm labels/disclaimer and clean exit. Host transforms and simulator navigation pass; physical UI still needs sign-off. | Cipher PORT_NOTES |
| OTP | Home → Tools → Tools → OTP Generator. Confirm real pad generation/page changes, RNG error presentation and cleanup over 50 cycles/ten minutes. Never use test pads for secrets. | OTP PORT_NOTES |
| Reader | Test `test_mixed_images.epub`, `test_reader_rendering_matrix.epub` and compound-selector fixture at every shipped font size; compare justification, margins, spacing and images after cold parse and reopen. Clear the relevant `/.crosspoint/epub_<hash>/` first. | PORT_LEDGER reader row; EPUB_COMPARISON |
| SecureStore | Time a ~4 KB decrypt at 50,000 iterations on C3; record latency, tune if required, reopen old blobs, and measure 50 crypto cycles. Do not ship its first consumer before this gate. | ACCEPTANCE, v2 foundations |
| Radio foundations | Acquire/release then open OPDS/transfer; test legacy-radio refusal, consecutive ownership, Back/sleep/Home exits, dense scan cap and ten-minute stability. | ACCEPTANCE, RadioManager |
| Migration | Each of eight sites needs its own screen plus next-screen radio test, ClockSync first, OTA last. OTA requires actual update and rollback. None migrated here. | RADIO_MIGRATION |
| Repartition/recovery | Establish stock-table baseline; serial-flash proposed table; verify books/settings and OTA between proposed-table builds; restore stock via supported update.bin path. Only then activate proposal. | PARTITION_DECISION five-step gate |
| General acceptance | Boot/battery, reading/page/font changes, sleep/wake/resume, launcher navigation and the five earlier apps' correctness/heap checks. Retain unchecked items until observed. | ACCEPTANCE |
| Future RF apps | Real traffic, PCAP correctness/drop counts and two-device message tests are conditional on those apps being implemented; not runnable now. | ACCEPTANCE Recon/Comms sections |
| X4 Pro firmware | A real board target and device verification are separate work; only x4-pro-simulator exists here. | AGENTS; PORT_LEDGER |

## Decisions needed

| Decision | Real options and trade-offs |
|---|---|
| WiFi/OPDS credentials | Require PIN before networking (stronger, changes unattended use); keep explicitly weak obfuscation (convenient, readable by attackers); random NVS key (better than public MAC, still vulnerable to flash extraction). SECURITY SEC-010 remains unresolved. |
| Medical Card | Public emergency data is accessible but exposes sensitive details; locked data protects privacy but may be unavailable to a responder; a limited public subset plus locked details needs explicit field choices. |
| BLE scope | Keep three BLE apps blocked, or approve adding a stack and measure C3 RAM/radio impact before porting. They are not permanently rejected solely by this session. |
| Missing hardware capabilities | Breadcrumb Trail/Vehicle Finder lack GPS; choose manual locations/notes or external data input, or cut those features. Emergency SOS lacks cellular; choose a precisely described local/network aid or omit it. No unsupported capability should be implied. |
| Security behavior | Define Ghost Mode's exact disabled services and recovery exception; choose what Security Sweep promises. Quick Wipe needs explicit data scope and confirmation without touching recovery. These are design prerequisites, not implemented protections. |
| Automation/task scope | Define useful, bounded settings tasks consistent with a dedicated e-reader before implementation; do not infer a general background execution system. |
| Network/offline gate | GOVERNANCE requires offline usefulness. Network-only proposals need a defined useful offline behavior or an explicit governance decision before intake. Radio migration alone does not waive this requirement. |
| Flash fallback | If physical repartition recovery fails: decide optional build tiles versus scope reduction. The proposed repartition is not active and has not been endorsed by a hardware result. |
| Release identity | Pick the release name before v1.0; GOVERNANCE still calls x4-merge a working name. |
