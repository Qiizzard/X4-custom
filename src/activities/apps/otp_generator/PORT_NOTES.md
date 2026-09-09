# Port notes — OTP Generator

**Source:** biscuit `src/activities/apps/OtpGeneratorActivity.{h,cpp}` (MIT)
**Tier:** `c3` · **Category:** Tools

This is a one-time-pad *codebook* page generator (a grid of random bytes to
write down and hand-encrypt against), not a TOTP authenticator — that app is
tracked separately in `docs/merge/PORT_LEDGER.md` under Tools → Security &
crypto ("Authenticator (offline TOTP)") and is still `todo`.

## Gate audit — what had to change

| Finding | Rule | Fix |
|---|---|---|
| `esp_fill_random()` via `<esp_random.h>` | build, 22 | Not available in the simulator (same class of issue `DiceRoller` hit), **and** unlike a dice roll, weak/predictable randomness here is a real defect — the pad's entire value is that it can't be predicted. Replaced with `mbedtls_ctr_drbg` seeded from hardware entropy via `mbedtls_entropy_func` — the same CSPRNG primitive `lib/SecureStore` already uses, on firmware only; the simulator reports RNG unavailable and does not test this entropy path. |
| No failure path if seeding the RNG fails | 16, 21 | The original had none to fail. Added: if `mbedtls_ctr_drbg_seed`/`_random` fail, the screen shows an explicit "random source unavailable" message instead of a page of zeroed or otherwise predictable bytes presented as if it were a real pad — showing fake-random data as real is exactly the security-theatre class rule 21 bans. |
| Pad bytes left in the object after use | good practice for a secret-producing app | `onExit()` wipes `pageData` with `mbedtls_platform_zeroize` before the entropy/DRBG contexts are freed. |
| No `tr()` on any user-facing string | 18 | `STR_APP_OTP_GENERATOR`, `STR_OTP_HINT`, `STR_OTP_RNG_ERROR`, `STR_OTP_NEW/PREV/NEXT`. |

## Kept from the original
The 10×10 hex-byte grid, paginated with Left/Right (Up/Down also works),
Select regenerates the current page.

## Verify on hardware
Tools → OTP Generator. Confirm the grid fills with hex byte pairs, Select
regenerates them, Left/Right moves between pages (each page independently
random — not a deterministic function of the page number). Open and close the
app repeatedly; free heap must return to baseline (the entropy/DRBG contexts
are stack members freed in `onExit()`, so there is nothing to leak, but the
mbedtls internals do their own heap work during seeding — this is exactly
what `ACCEPTANCE.md`'s SecureStore heap-cycle check already covers for the
same primitive).

## Gate record — audited 2026-09-05

**Status: wip.** Previous ~100-second holds were not the required ten-minute
soak. Older narrative above describes prior work, not fresh gate evidence.
The dated session report is the current verification record.

| Check | Current result |
|---|---|
| Registry and source | Present; simulator smoke walks entry/render/exit |
| Activity object | 832 bytes measured from C3 factory allocation; manifest updated |
| RAM budget | Declared-budget checker passes; runtime peak still needs validation where noted below |
| Builds | See `docs/merge/SESSION_REPORT_2026-09-05.md` for current C3/simulator results |
| Static analysis | Deferred to CI on Apple Silicon; not claimed run locally |
| Soak | Full 50-cycle/600000-ms entry-screen test recorded in session report; does not cover all interactions |
| Remaining checks | On real X4, generate and change pad pages, verify real entropy path and failure handling, and measure memory over 50 opens/closes plus ten minutes. Simulator only covers RNG-unavailable. |
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

### Batch 4 — 2026-09-08 UTC

Simulator resource review and button checks recorded in
`docs/merge/verification/BATCH_4_BASIC_APPS_2026-09-08.md`. Game of Life
known-pattern/wrap/counter tests pass; Clock/OTP still exercise unavailable
capability paths only. Physical and C3 resource gates remain open; wip.
