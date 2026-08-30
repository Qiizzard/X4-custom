# SECURITY — status of the audit's security findings

`RULESET.md` rule 21: a security-labelled feature **either works and is
reviewed, or it is removed.** No dead code presented as protection.

That rule applies to this file too. Its v1 revision described `SecureStore`,
`PasswordStore`, `DuressManager`, `RingBuffer` and a fixed `PacketMonitor` as
shipped and verified. None of them existed in the tree. A security document
that reports fixes which were never made is the same failure class as the code
it was written to police, so every entry below now states what is in the tree,
what is not, and how to check.

**Convention:** ✅ done and tested · ⚠️ partly done · ❌ open · 📋 standing policy.

---

## SEC-010 — credential store used reversible XOR — ⚠️ PARTLY FIXED

**The finding.** Credentials were "encrypted" with `data[i] ^= efuse_mac[i]`: a
reversible XOR keyed on the device's MAC address, which the device broadcasts in
every frame it transmits. Anyone who saw one packet could decrypt the file.

**What is now in the tree.** `lib/SecureStore` — AES-256-GCM for confidentiality
*and* integrity, with the key derived from a user passphrase by
PBKDF2-HMAC-SHA256 against a per-blob random salt. Every blob carries a fresh
salt and IV, and the whole 60-byte header (salt, iteration count, payload
length) is fed to GCM as additional authenticated data, so those fields cannot
be edited — in particular, an attacker cannot downgrade the KDF cost to one
iteration to make guessing cheap.

Verified by `test/secure_store/` (19 GoogleTest cases, in CI via the
`native-tests` job), which covers: round-trip; plaintext absent from the
ciphertext; wrong passphrase rejected with no partial plaintext returned; a
single-bit flip at **every byte offset** detected; iteration-count downgrade
detected; over-long length field rejected without an overread; salt and IV fresh
per encryption; and duress separation (below).

The host tests link a pinned mbedtls 3.6.2, the same major version ESP-IDF
builds into the firmware, so the tested code path is the shipped one.

**What is NOT fixed.** `lib/Serialization/ObfuscationUtils` is still present and
still in use by `src/WifiCredentialStore.cpp` and `src/OpdsServerStore.cpp`.

This is not an oversight, and `SecureStore` cannot simply be dropped in. Those
credentials must be readable **unattended at boot** so the device can reconnect
on its own; a passphrase-derived key would mean typing a PIN on every wake
before WiFi worked. The available options are all trade-offs, and picking one is
a product decision, not a refactor:

1. **Require a PIN to unlock networking.** Honest and strong; changes daily use.
2. **Keep device-bound obfuscation, relabel it.** Stop calling it encryption
   anywhere in the UI, docs, or field names, and say plainly that it stops
   casual reading of the SD card and nothing else. Cheapest, and still an
   improvement over a false claim.
3. **Keep a device-bound key but make it a real one** — a random key in NVS
   rather than the MAC. Beats XOR-on-MAC (the key is no longer broadcast) but
   still yields to anyone who can read flash. Not a secure element; do not
   describe it as one.

Until that is decided, treat WiFi and OPDS passwords on the SD card as
**readable by anyone holding the card**. Option 2's relabelling should land
regardless of which is chosen, because the current wording overclaims today.

## SEC-004 — duress PIN path was dead — ❌ OPEN (mechanism proven, nothing wired)

**The finding.** A duress PIN existed but nothing ever invoked it: a protection
users could configure and that would never fire.

**What is now in the tree.** Only the cryptographic half, and it is sound:
`SecureStore` makes the real vault and the decoy vault two blobs under two
passphrases, each authenticating its own and failing on the other
(`DuressPinOpensOnlyTheDecoyVault`). That matters because it means the decoy is
not a UI trick — someone dumping the SD card cannot tell which blob is which,
and the duress PIN genuinely cannot decrypt the real vault.

**What is NOT in the tree.** `DuressManager`, `SecurityPinActivity`, and the
`PasswordStore` that would consume them. There is no PIN entry, no duress
detection, no path redirection, and no wipe-after-N-failures. Until the PIN
Security app lands (`PORT_LEDGER.md`, Defense tile), **there is no duress
feature at all** — which is at least an honest absence rather than a dead path
presented as protection.

## SEC-006 / SEC-007 — TLS validation disabled — 📋 POLICY (nothing offending ported)

biscuit's network layer calls `setInsecure()`, disabling certificate validation.
No app carrying that code has been ported. The standing rule (RULESET 22):

- HTTPS uses `esp_crt_bundle_attach`. **Never** `setInsecure()`.
- Replace it at port-intake time, or cut the feature. This binds the Tools →
  Network tile (HTTP Client especially) before any of it lands.
- CrossInk's own network code still needs the same audit; it has not been done.
  `src/network/HttpDownloader.cpp` is the place to start.

## MEM-001..004 — heap/SD work inside radio callbacks — ⚠️ PRIMITIVE READY, NO CONSUMER

**The finding.** The audit's #1 crash class: allocating, sorting, taking mutexes
and writing to SD from inside a WiFi promiscuous callback.

**What is now in the tree.** `lib/RingBuffer` — the lock-free SPSC framed ring
that rule 10 makes mandatory for those callbacks. Storage is an in-object array,
so there is no allocation to fail; overflow drops the incoming frame and counts
it rather than overwriting queued ones, so a capture app can report honestly how
much it lost. 11 GoogleTest cases in CI, including a concurrent
producer/consumer run that asserts every frame the producer accepted comes back
byte-identical and in order.

`RadioManager::startPromiscuous()` enforces the pattern at the boundary: its
callback trampoline reads a header, hands over a pointer and a length, and
returns. It allocates nothing.

**What is NOT in the tree.** `PacketMonitor` itself, or any other capture app.
The primitive and the discipline are ready; nothing uses them yet.

## RADIO — state leaks between screens — ⚠️ ARBITER LANDED, LEGACY UNMIGRATED

Not a numbered audit finding, but the same class. `lib/RadioManager` provides
single-owner arbitration with symmetric acquire/shutdown (rules 7-9). CrossInk's
own eight network call sites still drive Arduino WiFi directly;
`acquire()` detects that (`foreignRadioActive()`) and refuses rather than taking
the antenna from a live download. Migration order and per-site risk are in
`RADIO_MIGRATION.md`. **`RadioManager` has not yet been exercised on hardware.**

---

## How to re-check any claim on this page

```bash
cmake -S test -B build/test -DCMAKE_BUILD_TYPE=Release
cmake --build build/test -j
ctest --test-dir build/test --output-on-failure   # RingBuffer + SecureStore included
```

For the ✅/⚠️ split above: a claim is only ✅ when a test in that run proves it.
Everything else says what is missing, and where.
