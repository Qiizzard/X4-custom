// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
#include "OtpGeneratorActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#ifndef SIMULATOR
#include <mbedtls/platform_util.h>
#endif

#include <cstdio>
#include <cstring>

#include "Logging.h"
#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr char kDrbgPersonalization[] = "x4merge-otp-pad";
}

bool OtpGeneratorActivity::seedRng() {
#ifdef SIMULATOR
  // No mbedtls in the simulator link -- report unavailable rather than fake
  // randomness (rule 21). generatePage()'s !rngReady path already draws the
  // honest "unavailable" state instead of a plausible-looking fake pad.
  return false;
#else
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&drbg);
  const int result = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
                                           reinterpret_cast<const unsigned char*>(kDrbgPersonalization),
                                           sizeof(kDrbgPersonalization) - 1);
  if (result != 0) {
    LOG_ERR("OTP", "mbedtls_ctr_drbg_seed failed: %d", result);
    return false;
  }
  return true;
#endif
}

void OtpGeneratorActivity::onEnter() {
  Activity::onEnter();
  pageNumber = 0;
  rngReady = seedRng();
  generatePage();
  requestUpdate();
}

void OtpGeneratorActivity::onExit() {
  // Reverse order of acquisition (rule 14). Wipe pad bytes before freeing --
  // they are the entire secret this app exists to produce. (Simulator builds
  // never populate real pad bytes -- seedRng() above always fails there -- but
  // the wipe stays unconditional so both builds free/clear symmetrically.)
#ifdef SIMULATOR
  std::memset(pageData, 0, sizeof(pageData));
#else
  mbedtls_platform_zeroize(pageData, sizeof(pageData));
  mbedtls_ctr_drbg_free(&drbg);
  mbedtls_entropy_free(&entropy);
#endif
  rngReady = false;
  Activity::onExit();
}

void OtpGeneratorActivity::generatePage() {
  if (!rngReady) {
    // Never draw zeroed/predictable bytes as if they were a real pad --
    // that is exactly the "security theatre" rule 21 exists to ban. rngReady
    // is unconditionally false in SIMULATOR builds (see seedRng()), so this
    // is the only branch that ever runs there.
    std::memset(pageData, 0, sizeof(pageData));
    return;
  }
#ifndef SIMULATOR
  if (mbedtls_ctr_drbg_random(&drbg, pageData, sizeof(pageData)) != 0) {
    LOG_ERR("OTP", "mbedtls_ctr_drbg_random failed");
    rngReady = false;
    std::memset(pageData, 0, sizeof(pageData));
  }
#endif
}

void OtpGeneratorActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Left) ||
      mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    if (pageNumber > 0) {
      --pageNumber;
      generatePage();
      requestUpdate();
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Right) ||
      mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    ++pageNumber;
    generatePage();
    requestUpdate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    generatePage();
    requestUpdate();
  }
}

void OtpGeneratorActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  char title[40];
  snprintf(title, sizeof(title), "%s - %lu", tr(STR_APP_OTP_GENERATOR), static_cast<unsigned long>(pageNumber) + 1);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, header, title, false);
  } else {
    GUI.drawHeader(renderer, header, title);
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();

  if (!rngReady) {
    renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() / 2, tr(STR_OTP_RNG_ERROR), true);
  } else {
    const int startY = header.y + header.height + metrics.verticalSpacing;
    const int cellW = (pageWidth - metrics.contentSidePadding * 2) / kCols;
    const int cellH = renderer.getLineHeight(SMALL_FONT_ID) + 6;

    for (int row = 0; row < kRows; ++row) {
      for (int col = 0; col < kCols; ++col) {
        const int idx = row * kCols + col;
        char hex[4];
        snprintf(hex, sizeof(hex), "%02X", pageData[idx]);
        const int x = metrics.contentSidePadding + col * cellW;
        const int y = startY + row * cellH;
        renderer.drawText(SMALL_FONT_ID, x, y, hex);
      }
    }

    renderer.drawText(SMALL_FONT_ID, metrics.contentSidePadding, startY + kRows * cellH + metrics.verticalSpacing,
                      tr(STR_OTP_HINT));
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_OTP_NEW), tr(STR_OTP_PREV), tr(STR_OTP_NEXT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
