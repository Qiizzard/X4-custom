# Port notes — Cipher Tools

**Source:** biscuit `src/activities/apps/CipherActivity.{h,cpp}` (MIT)
**Tier:** `c3` · **Category:** Tools

**This is a toy.** ROT13/Caesar/Vigenere/XOR/Atbash/Base64 are classroom and
puzzle transforms with zero real confidentiality value; XOR keyed on a short
text key is not encryption. Rule 21 says a security-labelled feature either
works and is reviewed, or it is removed — the fix here is the other option
this app was always meant to take: label it honestly as a toy instead of
protection, and never let it look like `lib/SecureStore`. The disclaimer is
drawn on the algorithm-selection screen, not only noted in a PR.

## Gate audit — what had to change

| Finding | Rule | Fix |
|---|---|---|
| `std::string inputText/keyText/result` members, cipher functions returning `std::string` built with `+=` | 1, 3 | Split into `Cipher.h/.cpp` — Arduino-free, UI-free, host-tested (`test/cipher/`) — operating on caller-supplied `char*` buffers with an explicit capacity, same shape as `MorseCode.h`. The activity holds fixed `char[]` fields only. |
| Raw XOR ciphertext bytes drawn straight to the screen as "text" | new finding, not in the original checklist | XOR ciphertext is not printable ASCII in general. The original biscuit app would have handed `renderer.drawCenteredText()` an arbitrary byte string. Split into two directions, both hex-safe: `XorEncryptHex` (text → hex ciphertext) and `XorDecryptHex` (hex ciphertext → text), so encoded ciphertext is printable hex. Decoded bytes are not guaranteed printable; binary/incorrect-key display remains a limitation. |
| `Base64Decode` silently skipped any character outside the base64 alphabet | 16 | A mistyped ciphertext then decoded to a *different, plausible-looking* plaintext instead of an error — the same failure class Morse Code's port fixed for truncation. Now strict: any non-alphabet character (beyond trailing `=` padding), wrong length, or bad padding count returns failure (empty result), never a guess. |
| Vigenere indexed into an unsanitized key (`(key[i]\|32)-'a'`), garbage for a non-alphabetic key | 16 | Sanitizes the key to its alphabetic characters first; a key with no letters at all now fails cleanly instead of indexing on nonsense. |
| No overflow reporting — `std::string` just grows | 1, 16 | Every transform takes an explicit `outCapacity` and returns 0 (buffer left empty) on overflow, matching `MorseCode`'s contract. |
| No `tr()` on any user-facing string | 18 | `STR_APP_CIPHER`, `STR_CIPHER_DISCLAIMER`, `STR_CIPHER_ENTER_TEXT/KEY/SHIFT`, `STR_CIPHER_INPUT_LABEL/RESULT_LABEL`, `STR_CIPHER_RUN/NEW`, `STR_CIPHER_INVALID`. |
| Ad hoc header/list drawing, no touch-header path | 19 | Routed through `TouchHeaderBackButton` + `GUI.drawList`/`drawButtonHints`, matching the other ported apps. |

## Kept from the original
The seven-ish classic transforms and the select → input → (key) → result
flow. Caesar's shift field still parses free-typed text via the shift/key
prompt; a non-numeric entry now explicitly no-ops (shift 0) and decimal shifts are reduced modulo 26 during parsing to avoid overflow.

## Verify on hardware
Tools → Cipher Tools. Confirm the toy disclaimer is visible on the algorithm
list. Run ROT13 on a short phrase, confirm it round-trips back through ROT13
a second time. Run XOR Encrypt with a key, confirm the result is hex only,
then run XOR Decrypt on that hex with the same key and confirm you get the
original text back. Run Base64 Encode/Decode round trip. Try Base64 Decode on
obviously-invalid input (e.g. `not valid base64!!`) and confirm it reports
failure rather than showing a wrong answer.

## Gate record — audited 2026-09-05

**Status: wip.** Previous ~100-second holds were not the required ten-minute
soak. Older narrative above describes prior work, not fresh gate evidence.
The dated session report is the current verification record.

| Check | Current result |
|---|---|
| Registry and source | Present; simulator smoke walks entry/render/exit |
| Activity object | 664 bytes measured from C3 factory allocation; manifest updated |
| RAM budget | Declared-budget checker passes; runtime peak still needs validation where noted below |
| Builds | See `docs/merge/SESSION_REPORT_2026-09-05.md` for current C3/simulator results |
| Static analysis | Deferred to CI on Apple Silicon; not claimed run locally |
| Soak | Full 50-cycle/600000-ms entry-screen test recorded in session report; does not cover all interactions |
| Remaining checks | Drive input/key/result/cancel for each algorithm on buttons; confirm translated labels and permanent toy disclaimer. Measure peak memory including KeyboardEntry. |
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

### Batch 3 — 2026-09-08 UTC

All eight menu entries now have simulator button coverage for input, keyed
entry where applicable, cancellation and retry; result Back also exercised.
14 host transform tests pass. See docs/merge/verification/BATCH_3_CIPHER_2026-09-08.md
for exact coverage limits and resource review. Physical display/labels and
peak keyboard allocations remain open; status stays wip.

### Integration review — 2026-09-09

UBSan reproduced signed overflow for Caesar key 2147483647; decimal shifts
now reduce modulo 26 while parsing. Malformed hex/Base64 failures now clear
partial output. Two new regressions supplement the 14 earlier tests.
Decoded binary bytes are still not guaranteed displayable, and equal-length
noncanonical Base64 padding bits are not checked. These toy transforms are
not security protections; the app remains wip for documented gates.
