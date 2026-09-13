# P4 encrypted file adapter — implementation checkpoint, unverified

EncryptedVaultFile connects the existing SecureStore implementation to HalStorage.
Password Manager is now registered as an experimental Tools app. The following
adapter and input sections describe its foundations; current integration is below.

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

At this earlier checkpoint the consumer was not registered; integration follows.
V1 must exercise record malformed input/limits/replacement/deletion and input
cancel/delete/timeout/forced exits, plus consumer lifecycle before release.

## Password Manager integration (2026-09-13, wip/unverified)

Tools now exposes create/repeat-passphrase, unlock, eight-entry listing,
add/replace/delete confirmation, and password reveal hidden again after ten
seconds. Adapted from Biscuit PasswordManager/PasswordDetail (MIT, Dave Allie,
2025); its obfuscated JSON store is deliberately replaced by SecureStore.
No import of that insecure format, password generator or passphrase change UI.

Use test credentials only until V1 and hardware gates clear. Passphrases are
8–64 printable ASCII characters; record limits are as above. All fields use
fixed masked input; replace re-enters all three fields. Up/Down selects rows,
Confirm opens/reveals, Page Forward edits, Page Back deletes, Up returns from
detail to list. Back locks and discards a pending edit; from locked/error it
exits. Input cancellation/timeout locks the parent. Unlocked main screens lock
after 60 seconds idle. Frontlight overlays are disabled in vault/input so they
cannot suspend the idle checks. Global Home destroys child then parent.

The activity has approximately 3.8 KB of fixed secret/model/buffer storage,
allocated by the fallible app factory, not on the task stack or globally.
Secret input uses one small fallible child activity. Crypto contexts allocate
as described in SecureStore; peak/KDF timings remain unmeasured. The master
passphrase remains only in the unlocked activity to encrypt edits. Records,
entry/draft, key and encoded buffers are wiped on lock/failure/exit/destruction.
Secret input now also serializes changes against rendering. No plaintext is
passed through KeyboardResult or logs. Exit clears the RAM framebuffer; RAM
wiping and redraw do not guarantee physical e-ink erasure or tamper resistance.

Files live under `/crossink/vaults/`: `passwords.bin` is primary. For edits,
`passwords.next` is exclusively created, encrypted, synced and closed first.
Then any old `passwords.previous` is removed, the primary renamed to previous,
and next renamed to primary. One prior encrypted version remains, including
potentially deleted entries: deletion is not secure erasure. There is no
claim of FAT rename atomicity, rollback protection, or crash-proof durability.
Any save error locks; next present or primary missing with previous present
blocks normal entry. No automatic fallback, overwrite, cleanup, or recovery UI.
Preserve these files for explicit recovery; do not treat an older backup as
current without checking it. Wrong keys/corruption also fail closed; exit and
reopen to retry. SecureStore simulator crypto remains unavailable, not faked.

Hardware verification (not run): on X3/X4, Tools > Password Manager, create
with test passphrase, add an entry, lock/reopen/unlock and verify its values;
replace/delete and repeat. Check wrong key, cancel/timeout/Home exits, ten-second
reveal masking, SD removal/write failure, and interrupted saves with all three
files preserved. Check physical display clearing, C3 KDF time, heap and stack
high-water marks. No cache reset is needed. V1 also needs fault injection and
record/input lifecycle suites. Hardware recovery and crypto gates stay open.

Compile evidence: C3 default PASS (152.050s), `/tmp/x4-p4c-build.log`,
2026-09-13 UTC. Consumer is now linked: firmware 6,378,976 bytes, 174,624
bytes free in stock OTA slot, below the reserve. No runtime tests performed.
