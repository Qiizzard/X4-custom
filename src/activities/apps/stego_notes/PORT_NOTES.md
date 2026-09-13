# Stego Notes — integrated, wip/unverified

EncryptedBmp adapts Biscuit SteganographyActivity's trailing-payload carrier
format (MIT, Copyright 2025 Dave Allie). It appends SCV1 + little-endian
ciphertext length + a SecureStore blob after the BMP-declared end. This is
discoverable concealment, not pixel steganography; encryption protects the
note. The visible image is not authenticated, and the encrypted trailer can
be copied between carriers without changing its note. No claim of carrier
integrity, secure deletion, or legacy plaintext STEG compatibility.

Notes are 1–512 bytes; passphrases 8–64 bytes. A 572-byte caller-owned workspace
is reused and wiped after operations, avoiding large task-stack allocations.
The copy buffer is 128 bytes; no framebuffer or app-level heap allocation.
SecureStore retains its existing crypto-context allocation requirements.
Caller buffers/paths/passphrase/length metadata must be disjoint; caller must
wipe successful plaintext on lock/exit. Invalid output bounds are rejected
without trusting them for a wipe. Header checks bound the envelope; they do
not certify every BMP pixel format or decoder behavior.

Input uses direct ASCII .bmp/.BMP filenames under /crossink/drawings/ and
must have no pre-existing trailer. New outputs and reveal use /crossink/stego/;
parent directories must exist. Carriers are bounded to 4 MiB. Exclusive create
never overwrites an existing file. All short read/write, sync and close failures
fail the operation; incomplete newly created output is removed if possible.
Cleanup failure leaves that file for explicit recovery. The original survives.

The registered UI is described below. V1 must exercise round trips, wrong keys,
malformed/oversized/truncated envelopes, allocation/entropy failure, write/sync/
close failure, existing-output protection and secret lifetimes. Hardware: on
X3/X4, use an Etch-A-Sketch BMP, embed a disposable note into a new output,
verify the original and visible image remain unchanged, then unlock the note;
repeat with wrong key and removed SD. No cache reset needed. These checks are
pending V1/device validation. Hardware crypto/recovery gates remain open.

C3 compile sanity PASS, final correction build 34.504s, log
`/tmp/x4-p4e-build-final.log` (2026-09-13). File-size checks use the 64-bit HAL
API to reject oversized trailing data without C3 size_t wrap. The unused helper
is discarded at link: image unchanged at 6,382,192 bytes, stock OTA headroom
171,408 bytes. No runtime/host/simulator/hardware tests passed or claimed.

## UI integration (2026-09-13)

Tools > Stego Notes offers embed and reveal. File scans examine at most 512
entries and show at most 16 BMP names (direct ASCII names, 63 bytes maximum).
The folder path is displayed; a scan-cap message appears when bounded early.
Embed reads `/crossink/drawings`, including Etch-A-Sketch outputs. Notes append
in masked chunks of up to 64 printable ASCII characters, newline-separated,
to a 512-byte total; Back discards. There is no existing-note editing/import.
Passphrases use fixed-buffer input, with repeat confirmation before writing.
Outputs are exclusive new `/crossink/stego/note-00.bmp` through `note-99.bmp`;
all occupied means failure, never replacement. The created path is displayed.

Reveal accepts the encrypted format only, validates printable ASCII/newlines,
and displays 64-byte pages with Up/Down navigation. Newlines appear as spaces
in the bounded four-line view. Revealed notes disappear after ten seconds,
even while paging. Other non-menu states reset after 60 seconds idle; input
children also expire after 60 seconds. Cancellation, Back, failure and exit
wipe note/key/entry/workspace; exit clears the RAM framebuffer. Frontlight
panels cannot suspend this flow. Physical e-ink remanence remains unverified.

Fixed activity storage is approximately 2.5 KB (including 1040-byte filenames,
513-byte note and 572-byte workspace), allocated through the existing fallible
launcher factory instead of using the task stack/global storage. Input uses
one small fallible child. No plaintext KeyboardResult/std::string copies.
The note display uses a 17-byte local line, wiped immediately after drawing.
Crypto allocations and device peak/stack/teardown behavior remain V1 work.

Device route: make/export an Etch-A-Sketch drawing, then Tools > Stego Notes >
Embed; select it, append disposable text, choose Encrypt/write, enter/repeat a
test passphrase. Verify the new output path, original image and unchanged visible
pixels. Return to Reveal, select output, check correct/wrong keys, page navigation
and ten-second wipe. Repeat cancellation, global Home, idle expiry, SD failure,
full output-name range and 512-byte boundaries. No device was flashed or tested.

Integration compile evidence: C3 default PASS (135.161s),
`/tmp/x4-p4f-build.log`, 2026-09-13. Helper and UI now linked: image 6,389,472
bytes, stock OTA free 164,128 bytes, reserve warning remains. All runtime,
crypto/file fault, simulator, soak and physical checks remain deferred.
