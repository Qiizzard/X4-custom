# ENVIRONMENT — what can and cannot be verified locally

Gaps between the gate `PORT_CHECKLIST.md` demands and what a given machine can
actually run. Recorded so a session does not silently tick a box it never
checked, and does not waste time rediscovering the same wall.

## macOS, Apple Silicon (the machine this merge is developed on)

| Check | Works? | Notes |
|---|---|---|
| `pio run -e default` (C3) | ⚠️ **only from a space-free path** | ESP-IDF aborts with `Detected a whitespace character in project paths`. A symlink does not help — the toolchain resolves the real path. Copy the tree somewhere without spaces and build there:<br>`rsync -a --exclude '.pio/' --exclude '.git/' ./ /tmp/x4build/ && cd /tmp/x4build && pio run -e default` |
| `pio run -e simulator` | ✅ | Native SDL2 build; the space in the path is fine. `brew install sdl2`. |
| `ctest` host suite | ✅ | `cmake -S test -B build/test -DCMAKE_BUILD_TYPE=Release && cmake --build build/test -j && ctest --test-dir build/test`. First run fetches googletest and mbedtls. |
| `budget_check.py` | ✅ | Needs `pyyaml`. PlatformIO's own venv has it: `~/.platformio/penv/bin/python tools/port/budget_check.py`. |
| `clang-format` | ✅ | `brew install llvm`; `.clang-format` needs v21+. The binary is not on `PATH` by default — `/opt/homebrew/opt/llvm/bin/clang-format`. |
| `pio check` (cppcheck) | ❌ **cannot run** | PlatformIO ships an **x86_64-only** cppcheck. Without Rosetta it fails with `Bad CPU type in executable`, and `pio check` hangs rather than reporting the error. Partial stand-in below. |
| `clang-tidy` on Arduino-free code | ✅ | `brew install llvm`. See below. |
| `pio run -e sticky` (S3) | ⚠️ same space-free-path rule as `-e default` | Also: **never interrupt a PlatformIO build.** A killed run leaves object files missing but the build tree looking current, and the next run fails at link with `cannot find ...o`. `rm -rf .pio/build/<env>` and rebuild. |

### Static analysis is CI-only here

There is no arm64 cppcheck in the PlatformIO package set, so the
`pio check --fail-on-defect low` box on `PORT_CHECKLIST.md` **cannot be ticked
from this machine**. Options, in order of preference:

1. **Let CI be the gate.** The `cppcheck` job runs on `ubuntu-latest` (x86_64)
   and is a required check, so nothing merges without it. This is sufficient —
   it is just slower feedback than running it locally.
2. Install Rosetta (`softwareupdate --install-rosetta`) and re-run `pio check`.
3. `brew install cppcheck` for a native binary and invoke it directly. It will
   be a different version than CI's, so treat disagreements as CI's call.

Until one of 2 or 3 is done, a PR from this machine should say **"static
analysis: deferred to CI"** rather than leaving the checklist box ambiguous.

### A usable stand-in: clang-tidy

`brew install llvm` puts `clang-tidy` alongside the `clang-format` the repo
already needs, and it runs natively on arm64. It cannot analyse the
Arduino/ESP-IDF translation units without a compile database, but it works
directly on the Arduino-free parts — which is where the risk concentrates
(crypto, the lock-free ring buffer, parsing and formatting):

```bash
/opt/homebrew/opt/llvm/bin/clang-tidy <file> --quiet \
  --checks='clang-analyzer-*,bugprone-*,cert-*,performance-*' \
  -- -std=gnu++2a -I <the file's own dir>
```

This is not a substitute for the cppcheck job — it does not see the activities
or anything touching the SDK — but it found two real defects in the first app
wave (an unterminated intermediate buffer, and a truncated number being
presented as a converter result), so it earns its place before a PR.

## Anything else

If you hit an environment wall that costs more than a few minutes, add a row
here. The point of this file is that nobody pays for the same discovery twice.

## Verification run, 2026-09-05

- Host is Darwin arm64. PlatformIO is at `~/.platformio/penv/bin/pio`; CMake
  and CTest are under `~/.platformio/packages/tool-cmake/bin/`.
- The inherited `build/test` cache refers to a former Linux session path.
  Use a fresh build directory; do not assume its old executables are native.
- C3 build succeeds from `/tmp/x4-batch5-firmware`, with the committed
  PlatformIO configuration and normal package downloads; no URL swaps or
  SDK-header workarounds were required. The build requires access to the
  shared PlatformIO package cache. Temporary build directories may be removed.
- The soak harness now supports native macOS allocator statistics. A
  100-second hold is insufficient regardless of whether its trend is flat.
