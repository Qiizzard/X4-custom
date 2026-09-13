# Stego Notes — source checkpoint, unverified

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

Next: registered file-selection, note/key input and reveal/lock UI; no usable
Stego Notes app is exposed yet. V1 must exercise round trips, wrong keys,
malformed/oversized/truncated envelopes, allocation/entropy failure, write/sync/
close failure, existing-output protection and secret lifetimes. Hardware: on
X3/X4, use an Etch-A-Sketch BMP, embed a disposable note into a new output,
verify the original and visible image remain unchanged, then unlock the note;
repeat with wrong key and removed SD. No cache reset needed. These checks are
pending the UI integration. Hardware crypto/recovery gates remain open.

C3 compile sanity PASS, final correction build 34.504s, log
`/tmp/x4-p4e-build-final.log` (2026-09-13). File-size checks use the 64-bit HAL
API to reject oversized trailing data without C3 size_t wrap. The unused helper
is discarded at link: image unchanged at 6,382,192 bytes, stock OTA headroom
171,408 bytes. No runtime/host/simulator/hardware tests passed or claimed.
