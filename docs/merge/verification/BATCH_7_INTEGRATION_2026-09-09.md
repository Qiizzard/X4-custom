# Batch 7 — integration, 2026-09-09 UTC

Integrated the reviewed implementation in three scoped commits:

- `99ca9764`: five offline app ports, registry/translations, Cipher host tests,
  measured object declarations and explicit wip ledger/port notes.
- `3c70ae7a`: app input tests, Game of Life rule assertions and opt-in lifecycle
  runner/harness. Earlier saved verification patches are now integrated;
  do not apply them again.
- `8994be8e`: Home menu count, portable bounded SSID copy and corrected
  credential-obfuscation comment (no credential-storage behavior change).

## Review findings fixed before integration

UBSan reproduced signed integer overflow for Caesar key `2147483647`:
`25 + 2147483647` in the letter-shift calculation. Parsing now reduces each
decimal digit modulo 26, including signed maximum-length keyboard inputs,
without heap allocation or an overflowing intermediate integer.

A second regression demonstrated that malformed hex/Base64 input left a
partially decoded prefix despite returning failure. Those paths now clear
out[0]. Both new regressions failed before the fixes and pass afterward.
The cipher remains a toy. Decoded binary bytes may be undisplayable, and
noncanonical Base64 padding bits remain a limitation; no authentication or
confidentiality guarantee is made.

## Verification

- 246/246 host tests PASS in a fresh native CMake build at
  /tmp/x4-batch7-host, using the locally cached GoogleTest/mbedtls sources.
- All 16 Cipher tests PASS under undefined-behavior sanitizer with recovery
  disabled. Earlier overflow and partial-output failures are retained in
  /tmp/x4-batch7-red-overflow.log and /tmp/x4-batch7-red-decode.log.
- Simulator build PASS (4.142 seconds); full input/reader/CSS/Game of Life
  smoke PASS, exit 0 and explicit success marker at 54493 ms.
- C3 default build PASS (36.238 seconds) in the space-free copy
  /tmp/x4-batch5-firmware. Image 6,316,352 bytes; 237,248 bytes free in the
  6,553,600-byte slot. The low-headroom warning remains.
- Declared RAM budget PASS; all 10 budget-checker self-tests PASS. This does
  not establish the still-unmeasured keyboard/crypto runtime peaks.
- Scoped formatting and staged whitespace checks passed. No generated/local
  files, firmware binaries, .agents files or proposed partitions were staged.

Logs: /tmp/x4-batch7-{host-build,ctest,cipher,simulator-build,smoke,default,budget,budget-tests}.log.
The build/test snapshot contains the integrated code; subsequent edits only
corrected comments/port notes and budget annotations. No repeated ten-minute
soak was needed for the allocation-free Cipher arithmetic/failure fixes or
formatting. Batch 1 remains entry-screen lifecycle evidence, not a fresh
full-app or hardware soak. Static analysis remains deferred to CI; no S3 or
physical verification is claimed.

Tested artifact SHA-256:

- Simulator: `41e450d60c9f6eecbf591fe6305393ff95a26fb41e844dd8e81554a34795b79b`
- C3 image: `4099500e9c075e11212efa1c4e4456ea6d03382cd173f56fd4eba9ae2643ed37`
- Cipher.cpp: `d729cc2ce81ade4bc3bb196caaf564ee50cd742d4c1af6b67805e20629b63617`

## Remaining work

All five new apps remain wip for the device/resource gates in their notes.
Batch 8 reconciles remaining inherited documentation, historical status
claims and the decision/hardware list. Local .agents, firmware-builds,
generated EPUB and proposed partition CSV remain unstaged. No firmware
source changes remain uncommitted after the integration commits.

On X4, verify all Home menu destinations; scan/select a 32-byte SSID and
confirm expected displayed/selected text. In Tools → Cipher, test large
positive/negative shifts and malformed hex/Base64 (visible invalid-input
state, no partial result). Follow the existing app-specific phone-scan,
RTC-capable-device, entropy, buttons/display and peak-memory procedures.
These checks need no EPUB cache reset. CSS/reader cold-cache checks retain
batch 5's per-book cache-reset instructions. No flash or partition change
was performed; hardware/product gates still govern further expansion.
