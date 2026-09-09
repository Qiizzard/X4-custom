#pragma once
// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
// OTP Generator -- tools app. Ported from biscuit through docs/merge/PORT_CHECKLIST.md.
//
// This is a one-time-pad *codebook* generator: each "page" is a grid of
// random bytes meant to be written down and used to hand-encrypt a message
// (classically, by XORing plaintext against pad bytes never reused). Unlike
// DiceRoller, a weak PRNG here would be a real defect -- the pad's secrecy is
// the entire point -- so this uses the same CSPRNG primitive as
// lib/SecureStore (mbedtls CTR-DRBG seeded from hardware entropy) rather than
// a fast, predictable generator, matching rule 22's approved crypto building
// blocks.
//
// CORRECTED (2026-08-31, first real simulator compile): the original comment
// here claimed mbedtls was "portable to both" firmware and simulator -- that
// was wrong, caught only once `-e simulator` actually compiled this file.
// The simulator link has no mbedtls (see docs/merge and lib/SecureStore's own
// notes: "SecureStore is deliberately absent [from the simulator]"). Under
// SIMULATOR, seedRng() below never links mbedtls and always reports the RNG
// unavailable -- generatePage()'s existing rule-21 fallback (never draw
// zeroed/predictable bytes as if real) already does the right thing with
// that, so the on-screen result is an honest "Random source unavailable"
// rather than a silent wrong build.
#ifndef SIMULATOR
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#endif

#include <cstdint>

#include "activities/Activity.h"

class OtpGeneratorActivity final : public Activity {
 public:
  explicit OtpGeneratorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("OtpGenerator", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr int kCols = 10;
  static constexpr int kRows = 10;
  static constexpr int kBytesPerPage = kCols * kRows;

  // In-object, fixed size (rule 1) -- no heap allocation for page data.
  uint8_t pageData[kBytesPerPage] = {};
  uint32_t pageNumber = 0;

  // Entropy/DRBG context. Allocated in onEnter(), freed in onExit() (rule
  // 14) -- these are not trivially destructible and must not outlive the
  // activity or leak across screens. Simulator builds never construct these
  // (no mbedtls there); rngReady simply stays false in that build.
#ifndef SIMULATOR
  mbedtls_entropy_context entropy{};
  mbedtls_ctr_drbg_context drbg{};
#endif
  bool rngReady = false;

  bool seedRng();
  void generatePage();
};
