# RULESET — engineering law for the X4 merge firmware

This is the non-negotiable ruleset every line of code — original or ported —
must satisfy. It is the merge of CrossInk's `AGENTS.md` discipline and the
failure classes catalogued in biscuit's `AUDIT_REPORT.md`. If a change can't
meet these, it doesn't ship.

> **The one-sentence version:** on a 380 KB single-core chip, *stability beats
> features* — every allocation is justified, the radio is always arbitrated,
> and nothing user-facing crashes or bricks.

---

## 0. Hardware truth

| Target | Chip | RAM | Notes |
|---|---|---|---|
| **X4 (base)** | ESP32-C3, 1 core, 160 MHz | ~380 KB SRAM, **no PSRAM** | the floor — shared code must fit here |
| **X4 Pro / Sticky** | ESP32-S3, 2 core | 8 MB PSRAM | extra headroom for `tier: psram` apps only |

- One 1-bit framebuffer, `800*480/8 = 48000` B, always resident. Never allocate a second.
- Use runtime renderer dimensions; **never hardcode `800` / `480`**.
- SD is `FsFile` (not Arduino `File`); only one file open at a time on real hardware.

## 1. Memory (the audit's #1 and #2 crash classes)

1. **Cap every collection.** No unbounded `std::vector`/list growth. Fixed size or a hard cap checked before every insert.
2. **`reserve()` before push loops.** No incremental reallocation in a loop.
3. **No `std::string` / Arduino `String` in hot paths.** Prefer `char[]`, `string_view`, `snprintf`.
4. **Big constant tables are `static const`** so they live in flash/DROM, not DRAM.
5. **Every app declares a budget** in `tools/port/app_budgets.yaml` and passes `budget_check.py`.
6. **`new` is not nothrow on ESP32.** With exceptions disabled, bare `new` calls `abort()`. Use `makeUniqueNoThrow<T>()` / `makeUniqueNoThrow<T[]>()` from `lib/Memory/Memory.h`, check for `nullptr`, `LOG_ERR` and fall back.

## 2. Radio (the audit's state-leak class)

7. **All WiFi/BLE goes through `RadioManager` (`RADIO`).** No activity calls `esp_wifi_*` / raw BLE directly.
8. **Acquire in `onEnter()`, release in `onExit()`** with `RADIO.shutdown()`. Acquisition and release must be symmetric — a leaked radio leaves the next screen with a dead antenna.
9. WiFi and BLE share one antenna. Do your work, release, don't hold it idle.

## 3. Concurrency & ISRs (the audit's data-race class)

10. **No allocation inside an ISR or radio/promiscuous callback.** No `push_back`, no `std::string`, no `std::sort`, no `malloc`. Callbacks may only copy raw bytes into a **fixed pre-allocated ring buffer** — use `lib/RingBuffer/RingBuffer.h` (lock-free SPSC, drops when full, unit-tested); parsing and storage happen later in `loop()`.
11. ISR handlers are `IRAM_ATTR`; ISR-read data lives in DRAM, not flash-only storage.
12. Never `xSemaphoreTake()` from an ISR — use the ISR-safe give APIs.
13. Delete FreeRTOS tasks before their activity is destroyed.

## 4. Activity lifecycle

14. Activities are heap-allocated and deleted on exit. Allocate long-lived buffers/tasks in `onEnter()`; free in **reverse order** in `onExit()`; close every file handle in `onExit()`.
15. Typical task stacks: 2048 B simple render work, 4096 B network/EPUB work. Keep local stack frames < 256 B unless justified.

## 5. Error handling

16. Recoverable failure → `LOG_ERR(...)` + `return false`, or a known safe fallback. **Always log before returning failure** from an allocation, file, parse, network, or hardware path.
17. `assert(false)` only for truly impossible states. `ESP.restart()` only for intentional recovery (e.g. finishing OTA). No exceptions, no stray `abort()`.

## 6. UI & i18n (one product, one voice)

18. **Every user-facing string goes through `tr(STR_*)`.** Logs may be hardcoded. Add keys to `lib/I18n/translations/*.yaml` and regenerate.
19. Draw through `UITheme` / `GUI` so fonts, spacing, and orientation stay consistent across the reader, the launcher, and every app. No app ships its own fonts or visual language.
20. Logical `MappedInputManager::Button::*` values in activities; raw button indices only in the mapping layer. The 7-button grammar means the same button does the same kind of thing everywhere.

## 7. Security (the audit's "security theatre" class)

21. A security-labelled feature **either works and is reviewed, or it is removed.** No dead code presented as protection.
22. Banned outright, per the audit: XOR "encryption" keyed on the broadcast MAC; TLS with certificate validation disabled; duress/PIN paths that are never actually invoked. Fix with real crypto and a threat-model note, or cut. **Approved building blocks:** encryption → `lib/SecureStore` (AES-256-GCM + PBKDF2, unit-tested); TLS → `esp_crt_bundle_attach`, never `setInsecure()`. Status of each finding is tracked in `SECURITY.md`.

## 8. Reliability of the device itself (don't brick)

23. **The OTA + SD-update recovery path is load-bearing and is never removed.** It is what makes a bad flash recoverable. Keep a documented stock-`update.bin` rollback.
24. The recovery path is tested every release (see `PORT_CHECKLIST.md` and Phase 5).

---

*If a rule here ever conflicts with an upstream (CrossInk/CrossPoint) change,
prefer this file and note the conflict in the PR. When in doubt: stability beats
features.*
