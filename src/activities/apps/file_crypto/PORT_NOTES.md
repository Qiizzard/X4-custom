# Small-file encryption — SD Encryption adaptation, wip/unverified

Uses existing SecureStore PBKDF2/AES-GCM, not Biscuit's iterative SHA/AES
format. No legacy import, verification-token file, directory encryption or
full-disk claim. Defense entry encrypts files from /crossink/to-encrypt into
/crossink/encrypted/file-00.bin through file-99.bin. Decryption reads that
folder and exports plaintext into /crossink/decrypted/file-00.bin..99.bin.
Generic output names do not preserve filename/extension metadata.

Plaintext cap 4096 bytes (empty allowed), encrypted cap 4156 including the
SecureStore header. Lists retain at most 16 names after at most 512 inspected
entries; names up to 64 bytes using letters/digits/dot/dash/underscore only,
no leading dot or subdirectories. Limits and empty lists are shown. One extra
scan-name byte detects truncation before retaining a name. Authentication
finishes before a decrypted file is created. Encryption asks/repeats a key
of at least eight characters. Source files remain unchanged, including any
plaintext originals; this limitation and plaintext exports are always shown.

Working buffers are activity-owned: 4096 plaintext + 4156 encrypted bytes,
1040 name bytes, two 65-byte secret buffers and a 96-byte output path: 9,518
fixed array bytes, excluding scalar/base/SDK overhead; not a measured heap peak. Fallible launcher
allocation fails before entry; buffers are reused, never on task stack, and
wiped after operation/cancel/error/exit/destruction. Secret input uses the
existing bounded child and its idle cancellation. No secrets render on screen.
Input closes before output opens. Exclusive output creation never overwrites;
write/sync/close are checked and failed output removal is best effort/logged.
Errors may leave a partial file, explicitly disclosed; no secure-erasure claim.

Deferred V1/device: round-trip empty/text/binary/4096-byte files and compare
bytes externally; reject 4097-byte plaintext and oversize/truncated/tampered
blobs; wrong key creates no plaintext output; repeat-key/cancel/timeout paths;
64/65-byte names, 16/512 scan caps, occupied 100 outputs, full/removed SD,
allocation/crypto failures, rotations and repeated entry/exit heap. Confirm
source and unrelated files unchanged. No cache reset; use disposable fixtures.
