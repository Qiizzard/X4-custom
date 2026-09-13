# P4 encrypted file adapter — implementation checkpoint, unverified

EncryptedVaultFile connects the existing SecureStore implementation to HalStorage.
It is not yet a registered app or a complete Password Manager. UI, record format,
editing/replacement/recovery and passphrase-entry lifetime handling remain P4 work.
Do not expose this as a usable vault before those pieces exist.

The caller supplies disjoint buffers (maximum 4096-byte plaintext and 4156-byte
blob), holds the passphrase only for an operation, and wipes successful plaintext
on lock/exit. Adapter scratch is wiped; valid output buffers are wiped on load
failure. Invalid buffer arguments are rejected before trusting capacities. All
buffers must not alias paths/passphrases/length metadata. There are no adapter
allocations; SecureStore's crypto contexts still allocate fallibly as documented.

Only app-owned filenames directly under `/crossink/vaults/` are accepted. Creation
requires an existing parent directory and uses O_EXCL; no overwrite or fallback
to unauthenticated data. Encryption succeeds before any file is created. Reads
are bounded and files are closed before decryption. Writes, sync and close are
checked. A failed creation may leave an incomplete file if cleanup also fails;
IoError is uncertain and must not trigger blind overwrite/retry. Existing vaults
are never deleted by this helper. Files use SecureStore's existing blob format.

C3 compile sanity is the only current check; the linker may discard this adapter
until a real app references it. No runtime crypto/file behavior is certified.
V1 needs wrong-passphrase/tampering, oversized files, alias/argument limits,
allocation/entropy failure, SD write/sync/close failure, exclusive-create races,
secret cleanup, and physical KDF timing/heap checks. Hardware work must follow
an actual consumer's create/lock/reopen path. No device action occurred here.

## Record and secret-input checkpoint (2026-09-13)

PasswordRecords owns eight records with 31-byte titles, 47-byte usernames and
63-byte passwords (UTF-8 byte limits). The 1160-byte PVR1 encoding is explicit:
magic, count, three reserved zeros, eight 144-byte fixed rows. Decode validates
all used fields before exposing records; failure clears the working set.
Replace supports self-field inputs via a 144-byte temporary, wiped afterward.
Erase wipes the vacated row; lock/destructor wipe all records. The caller must
keep encoded buffers disjoint and wipe them after encrypted file operations.
This codec must only receive authenticated plaintext from SecureStore.

SecretEntryActivity uses a fixed 65-byte draft and caller-owned destination,
not KeyboardResult/std::string for secrets. Printable ASCII character selection
supports Up/Down, Right add, Left delete, Page Forward jump ten, Confirm submit,
Back cancel; it expires after 60 seconds idle. Input is masked; selected next
character is visible. It is button-operated, not a full touch keyboard. The
parent must keep destination alive until child destruction, use a dedicated
entry buffer, enforce a suitable minimum and wipe it after use/lock/exit.
Only accepted submission copies plaintext into that buffer. Cancel/exit and
destruction wipe the draft. Hardware display remanence is not covered by RAM
wiping, and there is no claim of tamper-resistant memory or physical security.

Still missing: actual Password Manager activity, create/unlock confirmation,
revision save/recovery policy, integration of input cancellation with vault
locking, record editing and display lifetime. No new app is registered yet.
V1 must exercise record malformed input/limits/replacement/deletion and input
cancel/delete/timeout/forced exits, plus consumer lifecycle before release.
