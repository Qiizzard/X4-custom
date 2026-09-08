# Batch 3 — Cipher verification, 2026-09-08 UTC

Cipher remains **wip** pending physical and runtime-memory checks.

## Coverage

Final simulator build PASS (2.808 seconds). Final smoke PASS: exit 0 and explicit success marker at 44252 ms.
The button script walks all eight algorithms in their current menu order:
ROT13, Caesar, Vigenere, XOR encode/decode, Atbash, Base64 encode/decode.
It cancels input for each, submits input, cancels/reopens the key for the
four keyed entries, submits a key, renders the resulting activity, retries
from the result, cancels retry input, and eventually exits to Home. It also
checks Back from ROT13's result before reopening input. Press and release
are separate input frames. Activity-name assertions guard every transition.

The script uses digit-1 input/key data: invalid Vigenere/Base64/hex input can
intentionally reach the error-result path. This is navigation evidence, not
an assertion of private result-state fields, menu index, rendered text or
pixel correctness. It does not replace known-answer transform tests.

All **14 existing Cipher host tests PASS**, freshly compiled with Apple c++
(C++17, pthread) from test/cipher/CipherTest.cpp, Cipher.cpp and the cached
GoogleTest sources under build/test/_deps/googletest-src/googletest. No
transform or production activity code changed. Tests include known answers,
round trips, invalid input and bounded output. Full inherited host-suite and
C3 build results were not rerun for these simulator-only changes.

Commands: `pio run -e simulator`; then
`python3 scripts/run_simulator_smoke_test.py --no-build`.
The runner default timeout increases from 45 to 90 seconds: the expanded
button script already took 44.630 seconds before the additional result-Back
check, making the old timeout too tight. It retains exit-code/crash/success
marker checks. Logs: /tmp/x4-batch3-build.log, /tmp/x4-batch3-smoke.log,
/tmp/x4-batch3-host.log. Host binary: /tmp/x4-batch3-cipher-tests.

## Resource review

CipherActivity.h holds 129 input bytes, 33 key bytes and 257 output bytes,
419 bytes total, within the previously measured 664-byte C3 activity object.
These arrays do not allocate per transform. Cipher.cpp transforms operate
on caller-owned buffers; Vigenere uses a 33-byte local sanitized-key array.
No new framebuffer, persistent storage or radio resource is introduced.

KeyboardEntry is heap allocated while the Cipher activity stays on the
activity stack. Input and key keyboards are sequential: ActivityManager
exits/destroys the previous keyboard before invoking the result callback
(src/activities/ActivityManager.cpp:128). The moved input-result string may
still live while that callback creates the key keyboard. Titles, text,
render temporaries and callback storage add allocations beyond the fixed
activity arrays. Input is capped at 128 bytes and key at 32; those caps do
not establish the shared keyboard object's size or allocator overhead.

The manifest's **1024-byte peak-heap allowance is still an unverified
estimate**, not a measured bound or validated maximum. C3 keyboard object
size, dynamic-string capacities, peak heap, largest block and task-stack
margin remain open. The old C3 build artifacts are unavailable; the 664-byte
object size is prior evidence, not remeasured here. Checked outer keyboard
allocation does not make shared STL/internal allocations OOM-safe. No fault
injection, C3 peak measurement or memory improvement is claimed.

## Artifacts and integration

Verification used the inherited uncommitted firmware snapshot. Batch 7 owns
firmware integration. This batch publishes this record, the runner timeout
change and BATCH_3_CIPHER_SMOKE.patch. The patch is the Cipher-only addition
to batch 2's local smoke source; it requires Cipher registration/translation
keys and the earlier QR script helpers during integration. It is already in
the working file: do not apply twice. The unrelated CSS smoke change is not
part of this patch. No firmware-port completion claim follows from this commit.

SHA-256:

- Smoke source: 7a5ce878f8da95b03c0a3c18b34f326f2d809e2ea3ff3c80d4eed7cfb7d309c3
- Simulator: e18ee596af374cf2a60c56f3631a54f36c7894a1b532d62eb60b67ba7172e222
- CipherActivity.cpp: 67e688ae99cec4fef2fd8aa1e9a5af5ad47022b4ce12336f7843021baa9b2ad1
- Cipher.cpp: 7ebefaa02620f75a71a8ada208bf4723fe0fd1f2f6a3c12efa1427339bbc43cd

## Hardware follow-up

On X4: Home → Tools → Tools category → Cipher Tools. Check the toy disclaimer
on selection, translated labels, readable full input/result text, known-answer
ROT13/Caesar/Vigenere and XOR/Base64 round trips, invalid-input presentation,
Back from input/key/result, retry and final exit. Measure peak heap and largest
block at maximum input/key lengths and cleanup after repeated runs; record
render-task stack high-water marks. No EPUB cache reset is needed. These
physical checks and full interactive resource measurements remain open.
