# Port notes — QR Generator

**Source:** biscuit `src/activities/apps/QrGeneratorActivity.{h,cpp}` (MIT)
**Tier:** `c3` · **Category:** Tools

## Gate audit — what had to change

| Finding | Rule | Fix |
|---|---|---|
| Unbounded `std::string textPayload` member, `KeyboardEntryActivity` called with `maxLength = 0` (unlimited) | 1 | Replaced with a fixed `char payload[257]` and `maxLength = 256` on the keyboard entry — a bounded cap known at compile time, in the practical range for something re-typed by hand off a 7-button keyboard. `QrUtils::drawQrCode` itself already caps at `MAX_QR_CAPACITY` (2953 bytes, version-40/ECC-low), so this is a tighter, UI-appropriate bound, not a new one. |
| No `tr()` on any user-facing string | 18 | `STR_APP_QR_GENERATOR`, `STR_QR_ENTER_TEXT`, `STR_QR_NEW`. |
| Header drawn ad hoc (`GUI.drawHeader` at a hand-computed `Rect` with no touch-header path) | 19 | Routed through `TouchHeaderBackButton`, matching every other ported app so the header behaves identically on touch and non-touch hardware. |

## Kept from the original
The whole rendering path: this app does not reimplement QR generation. It
calls the same `src/util/QrUtils::drawQrCode()` that CrossInk's own reader
already ships (used for its WiFi/nearby-share QR screen) and already relies
on `ricmoo/QRCode`, which is already a `platformio.ini` dependency — nothing
new was added to the build. Also kept: `HalDisplay::FULL_REFRESH` on display,
since a QR code someone is about to scan wants a clean full-waveform refresh
more than it wants speed.

## A note on the buffer bridge
`QrUtils::drawQrCode()` takes `const std::string&` because it's already
shared, tested code from the base tree — not something this port owns to
rewrite. The activity itself holds `char payload[257]` (rules 1 and 3); a
bounded `std::string` is constructed from it once, at render time, only to
satisfy that existing signature. This is the same bridge pattern
`CountdownActivity`'s list-row lambda already uses to reach a
`std::string`-based shared component from a `char[]` field.

## Verify on hardware
Tools → QR Generator. Type some text, confirm the code renders and is legible
to a phone camera. Select opens the keyboard again pre-filled with the last
payload; initial empty input or Back exits the app. Empty input or Back while editing
retains the previous QR output.

## Gate record — audited 2026-09-05

**Status: wip.** Previous ~100-second holds were not the required ten-minute
soak. Older narrative above describes prior work, not fresh gate evidence.
The dated session report is the current verification record.

| Check | Current result |
|---|---|
| Registry and source | Present; simulator smoke walks entry/render/exit |
| Activity object | 488 bytes measured from C3 factory allocation; manifest updated |
| RAM budget | Declared-budget checker passes; runtime peak still needs validation where noted below |
| Builds | See `docs/merge/SESSION_REPORT_2026-09-05.md` for current C3/simulator results |
| Static analysis | Deferred to CI on Apple Silicon; not claimed run locally |
| Soak | Full 50-cycle/600000-ms entry-screen test recorded in session report; does not cover all interactions |
| Remaining checks | Scripted keyboard entry/output/edit-cancel/exit PASS. Phone-scan output and test initial cancellation. Measure peak memory including KeyboardEntry and QR buffers. |
| Hardware | Use the path described above; no cache reset for these apps |

The QR/Cipher keyboard object uses `makeUniqueNoThrow` with a checked failure
path where applicable. The shared keyboard still owns dynamic strings;
checking its outer allocation is not a guarantee against every possible OOM
inside shared UI infrastructure. No new resident buffer was added by the audit.

### Full lifecycle soak verified 2026-09-06

50 open/close cycles and a full 600000-ms hold PASS; final Home cleanup
passed within the 4096-byte tolerance. Entry-screen coverage only. See
`docs/merge/verification/BATCH_1_SOAK_2026-09-06.md` for measurements and
coverage limits. Other listed checks remain open; status stays wip.

### Batch 2 verification — 2026-09-08 UTC

Initial cancellation now passes the simulator script, alongside entry, output,
edit-cancel and exit. See `docs/merge/verification/BATCH_2_QR_2026-09-08.md`
for source-level heap/stack accounting and remaining C3 peak/OOM/phone-scan
gates. Status remains wip; older initial-cancel TODO above is superseded.
