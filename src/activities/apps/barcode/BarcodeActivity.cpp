// Adapted from biscuit, MIT, Copyright (c) 2025 Dave Allie.
// See PORT_NOTES.md for bounded input and pattern corrections.
#include "BarcodeActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

// ---------------------------------------------------------------------------
// Code 128 encoding table — Code Set B (indices 0-106)
// Each row: 6 alternating bar/space widths (bar, space, bar, space, bar, space).
// Stop symbol (index 106) physically has 7 bars/spaces but we store 6 here and
// append the trailing bar (width 2) manually in renderCode128().
// ---------------------------------------------------------------------------
static const uint8_t CODE128_PATTERNS[107][6] = {
    {2, 1, 2, 2, 2, 2}, {2, 2, 2, 1, 2, 2}, {2, 2, 2, 2, 2, 1}, {1, 2, 1, 2, 2, 3}, {1, 2, 1, 3, 2, 2},
    {1, 3, 1, 2, 2, 2}, {1, 2, 2, 2, 1, 3}, {1, 2, 2, 3, 1, 2}, {1, 3, 2, 2, 1, 2}, {2, 2, 1, 2, 1, 3},
    {2, 2, 1, 3, 1, 2}, {2, 3, 1, 2, 1, 2}, {1, 1, 2, 2, 3, 2}, {1, 2, 2, 1, 3, 2}, {1, 2, 2, 2, 3, 1},
    {1, 1, 3, 2, 2, 2}, {1, 2, 3, 1, 2, 2}, {1, 2, 3, 2, 2, 1}, {2, 2, 3, 2, 1, 1}, {2, 2, 1, 1, 3, 2},
    {2, 2, 1, 2, 3, 1}, {2, 1, 3, 2, 1, 2}, {2, 2, 3, 1, 1, 2}, {3, 1, 2, 1, 3, 1}, {3, 1, 1, 2, 2, 2},
    {3, 2, 1, 1, 2, 2}, {3, 2, 1, 2, 2, 1}, {3, 1, 2, 2, 1, 2}, {3, 2, 2, 1, 1, 2}, {3, 2, 2, 2, 1, 1},
    {2, 1, 2, 1, 2, 3}, {2, 1, 2, 3, 2, 1}, {2, 3, 2, 1, 2, 1}, {1, 1, 1, 3, 2, 3}, {1, 3, 1, 1, 2, 3},
    {1, 3, 1, 3, 2, 1}, {1, 1, 2, 3, 1, 3}, {1, 3, 2, 1, 1, 3}, {1, 3, 2, 3, 1, 1}, {2, 1, 1, 3, 1, 3},
    {2, 3, 1, 1, 1, 3}, {2, 3, 1, 3, 1, 1}, {1, 1, 2, 1, 3, 3}, {1, 1, 2, 3, 3, 1}, {1, 3, 2, 1, 3, 1},
    {1, 1, 3, 1, 2, 3}, {1, 1, 3, 3, 2, 1}, {1, 3, 3, 1, 2, 1}, {3, 1, 3, 1, 2, 1}, {2, 1, 1, 3, 3, 1},
    {2, 3, 1, 1, 3, 1}, {2, 1, 3, 1, 1, 3}, {2, 1, 3, 3, 1, 1}, {2, 1, 3, 1, 3, 1}, {3, 1, 1, 1, 2, 3},
    {3, 1, 1, 3, 2, 1}, {3, 3, 1, 1, 2, 1}, {3, 1, 2, 1, 1, 3}, {3, 1, 2, 3, 1, 1}, {3, 3, 2, 1, 1, 1},
    {3, 1, 4, 1, 1, 1}, {2, 2, 1, 4, 1, 1}, {4, 3, 1, 1, 1, 1}, {1, 1, 1, 2, 2, 4}, {1, 1, 1, 4, 2, 2},
    {1, 2, 1, 1, 2, 4}, {1, 2, 1, 4, 2, 1}, {1, 4, 1, 1, 2, 2}, {1, 4, 1, 2, 2, 1}, {1, 1, 2, 2, 1, 4},
    {1, 1, 2, 4, 1, 2}, {1, 2, 2, 1, 1, 4}, {1, 2, 2, 4, 1, 1}, {1, 4, 2, 1, 1, 2}, {1, 4, 2, 2, 1, 1},
    {2, 4, 1, 2, 1, 1}, {2, 2, 1, 1, 1, 4}, {4, 1, 3, 1, 1, 1}, {2, 4, 1, 1, 1, 2}, {1, 3, 4, 1, 1, 1},
    {1, 1, 1, 2, 4, 2}, {1, 2, 1, 1, 4, 2}, {1, 2, 1, 2, 4, 1}, {1, 1, 4, 2, 1, 2}, {1, 2, 4, 1, 1, 2},
    {1, 2, 4, 2, 1, 1}, {4, 1, 1, 2, 1, 2}, {4, 2, 1, 1, 1, 2}, {4, 2, 1, 2, 1, 1}, {2, 1, 2, 1, 4, 1},
    {2, 1, 4, 1, 2, 1}, {4, 1, 2, 1, 2, 1}, {1, 1, 1, 1, 4, 3}, {1, 1, 1, 3, 4, 1}, {1, 3, 1, 1, 4, 1},
    {1, 1, 4, 1, 1, 3}, {1, 1, 4, 3, 1, 1}, {4, 1, 1, 1, 1, 3}, {4, 1, 1, 3, 1, 1}, {1, 1, 3, 1, 4, 1},
    {1, 1, 4, 1, 3, 1}, {3, 1, 1, 1, 4, 1}, {4, 1, 1, 1, 3, 1}, {2, 1, 1, 4, 1, 2}, {2, 1, 1, 2, 1, 4},
    {2, 1, 1, 2, 3, 2}, {2, 3, 3, 1, 1, 1},
};

// ---------------------------------------------------------------------------
// Code 39 encoding table
// 44 valid characters: 0-9, A-Z, space, -, ., $, /, +, %
// Each entry is 9 bits: 5 bars + 4 spaces, 0 = narrow (1 unit), 1 = wide (3 units)
// Bit order: bar0, sp0, bar1, sp1, bar2, sp2, bar3, sp3, bar4
// ---------------------------------------------------------------------------
static const char CODE39_CHARS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-. $/+%";
// Each uint16_t encodes 9 elements as bits (0=narrow, 1=wide)
// MSB = element 0, bit 8 = element 8
static const uint16_t CODE39_PATTERNS[] = {
    // 0-9
    0b000110100,  // 0
    0b100100001,  // 1
    0b001100001,  // 2
    0b101100000,  // 3
    0b000110001,  // 4
    0b100110000,  // 5
    0b001110000,  // 6 (was 001110000)
    0b000100101,  // 7
    0b100100100,  // 8
    0b001100100,  // 9
    // A-Z
    0b100001001,  // A
    0b001001001,  // B
    0b101001000,  // C
    0b000011001,  // D
    0b100011000,  // E
    0b001011000,  // F
    0b000001101,  // G
    0b100001100,  // H
    0b001001100,  // I
    0b000011100,  // J
    0b100000011,  // K
    0b001000011,  // L
    0b101000010,  // M
    0b000010011,  // N
    0b100010010,  // O
    0b001010010,  // P
    0b000000111,  // Q
    0b100000110,  // R
    0b001000110,  // S
    0b000010110,  // T
    0b110000001,  // U
    0b011000001,  // V
    0b111000000,  // W
    0b010010001,  // X
    0b110010000,  // Y
    0b011010000,  // Z
    // - . SPACE $ / + %
    0b010000101,  // -
    0b110000100,  // .
    0b011000100,  // SPACE
    0b010101000,  // $
    0b010100010,  // /
    0b010001010,  // +
    0b000101010,  // %
};
// Start/Stop character is '*' encoded as:
static const uint16_t CODE39_STAR = 0b010010100;

// ---------------------------------------------------------------------------
// EAN-13 encoding tables
// L-code: used for left group based on parity, odd parity bars
// G-code: even parity, used in left group based on first digit parity table
// R-code: complement of L-code, used for right group
// Each value is a 7-bit pattern (MSB first), bar=1 space=0
// ---------------------------------------------------------------------------
static const uint8_t EAN13_L[10] = {
    0b0001101,  // 0
    0b0011001,  // 1
    0b0010011,  // 2
    0b0111101,  // 3
    0b0100011,  // 4
    0b0110001,  // 5
    0b0101111,  // 6
    0b0111011,  // 7
    0b0110111,  // 8
    0b0001011,  // 9
};
static const uint8_t EAN13_G[10] = {
    0b0100111,  // 0
    0b0110011,  // 1
    0b0011011,  // 2
    0b0100001,  // 3
    0b0011101,  // 4
    0b0111001,  // 5
    0b0000101,  // 6
    0b0010001,  // 7
    0b0001001,  // 8
    0b0010111,  // 9
};
static const uint8_t EAN13_R[10] = {
    0b1110010,  // 0
    0b1100110,  // 1
    0b1101100,  // 2
    0b1000010,  // 3
    0b1011100,  // 4
    0b1001110,  // 5
    0b1010000,  // 6
    0b1000100,  // 7
    0b1001000,  // 8
    0b1110100,  // 9
};
// Parity pattern for left group based on first digit (0=L, 1=G)
static const uint8_t EAN13_PARITY[10] = {
    0b000000,  // 0: LLLLLL
    0b001011,  // 1: LLGLGG
    0b001101,  // 2: LLGGLG
    0b001110,  // 3: LLGGGL
    0b010011,  // 4: LGLLGG
    0b011001,  // 5: LGGLLG
    0b011100,  // 6: LGGGLL
    0b010101,  // 7: LGLGLG
    0b010110,  // 8: LGLGGL
    0b011010,  // 9: LGGLLG (re-check: standard)
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void BarcodeActivity::drawPattern(int x, int y, int h, const uint8_t pattern[6], int unitW) const {
  int cx = x;
  for (int i = 0; i < 6; i++) {
    int w = pattern[i] * unitW;
    if (i % 2 == 0) {
      // Bar (filled black)
      renderer.fillRect(cx, y, w, h, true);
    }
    // Space: just advance (background is white)
    cx += w;
  }
}

// ---------------------------------------------------------------------------
// Activity lifecycle
// ---------------------------------------------------------------------------

void BarcodeActivity::onEnter() {
  Activity::onEnter();
  state = TYPE_SELECT;
  barcodeType = CODE128;
  inputText[0] = '\0';
  inputLength = 0;
  requestUpdate();
}

void BarcodeActivity::onExit() { Activity::onExit(); }

void BarcodeActivity::launchKeyboard() {
  const char* title = tr(STR_BARCODE_ENTER);
  if (barcodeType == EAN13)
    title = tr(STR_BARCODE_DIGITS);
  else if (barcodeType == CODE39)
    title = tr(STR_BARCODE_C39_ENTER);

  const size_t maxLength = barcodeType == EAN13 ? 13 : barcodeType == CODE39 ? 32 : 48;
  // The shared keyboard owns its strings; this parent retains only 49 bytes.
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, title, inputText, maxLength);
  if (!keyboard) {
    LOG_ERR("Barcode", "Cannot allocate keyboard");
    return;
  }
  startActivityForResult(std::move(keyboard), [this, maxLength](const ActivityResult& result) {
    const auto* entered = std::get_if<KeyboardResult>(&result.data);
    if (result.isCancelled || !entered || entered->text.empty()) {
      state = TYPE_SELECT;
    } else if (entered->text.size() <= maxLength && entered->text.size() < sizeof(inputText)) {
      inputLength = entered->text.size();
      memcpy(inputText, entered->text.data(), inputLength);
      inputText[inputLength] = '\0';
      if (barcodeType == CODE39)
        for (size_t i = 0; i < inputLength; ++i)
          if (inputText[i] >= 'a' && inputText[i] <= 'z') inputText[i] -= 'a' - 'A';
      state = SHOWING;
    } else {
      state = TYPE_SELECT;
      LOG_ERR("Barcode", "Keyboard result exceeds input bound");
    }
    requestUpdate();
  });
  state = TEXT_INPUT;
}

void BarcodeActivity::loop() {
  if (state == TEXT_INPUT) {
    // Keyboard sub-activity is running; nothing to handle here
    return;
  }

  if (state == TYPE_SELECT) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      finish();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
      barcodeType =
          static_cast<BarcodeType>((static_cast<int>(barcodeType) - 1 + BARCODE_TYPE_COUNT) % BARCODE_TYPE_COUNT);
      requestUpdate();
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
      barcodeType = static_cast<BarcodeType>((static_cast<int>(barcodeType) + 1) % BARCODE_TYPE_COUNT);
      requestUpdate();
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      launchKeyboard();
    }
    return;
  }

  if (state == SHOWING) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      finish();
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      state = TYPE_SELECT;
      requestUpdate();
    }
  }
}

// ---------------------------------------------------------------------------
// Render dispatch
// ---------------------------------------------------------------------------

void BarcodeActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  if (state == TYPE_SELECT) {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_APP_BARCODE));

    // Three centered rows for type selection
    const char* const TYPE_LABELS[BARCODE_TYPE_COUNT] = {
        tr(STR_BARCODE_C128),
        tr(STR_BARCODE_C39),
        tr(STR_BARCODE_EAN),
    };

    const int rowH = 52;
    const int listTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
    const int totalH = rowH * BARCODE_TYPE_COUNT;
    int startY = listTop + (pageHeight - listTop - metrics.buttonHintsHeight - totalH) / 2;

    for (int i = 0; i < BARCODE_TYPE_COUNT; i++) {
      const int y = startY + i * rowH;
      const int margin = 40;
      const int w = pageWidth - 2 * margin;
      const bool selected = (i == static_cast<int>(barcodeType));

      if (selected) {
        renderer.fillRect(margin, y - 4, w, rowH - 4, true);
        renderer.drawCenteredText(UI_10_FONT_ID, y + 8, TYPE_LABELS[i], false);
      } else {
        renderer.drawRect(margin, y - 4, w, rowH - 4, true);
        renderer.drawCenteredText(UI_10_FONT_ID, y + 8, TYPE_LABELS[i], true);
      }
    }

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), "^", "v");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  } else if (state == SHOWING) {
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_APP_BARCODE));
    renderBarcode();

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_BARCODE_NEW), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}

// ---------------------------------------------------------------------------
// Barcode rendering
// ---------------------------------------------------------------------------

void BarcodeActivity::renderBarcode() const {
  // Never silently drop unsupported characters while displaying the original label.
  for (char c : std::string_view(inputText, inputLength)) {
    if ((barcodeType == CODE128 && (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126)) ||
        (barcodeType == CODE39 && (c == 0 || !strchr(CODE39_CHARS, c)))) {
      renderer.drawCenteredText(UI_10_FONT_ID, renderer.getScreenHeight() / 2, tr(STR_BARCODE_INVALID));
      return;
    }
  }
  switch (barcodeType) {
    case CODE128:
      renderCode128();
      break;
    case CODE39:
      renderCode39();
      break;
    case EAN13:
      renderEan13();
      break;
  }
}

// --- Code 128B ---

void BarcodeActivity::renderCode128() const {
  int mt, mr, mb, ml;
  renderer.getOrientedViewableTRBL(&mt, &mr, &mb, &ml);
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  const int headerBottom = mt + metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int footerTop = pageHeight - mb - metrics.buttonHintsHeight - metrics.verticalSpacing;

  // Build symbol sequence: [StartB] [data chars] [checksum] [Stop]
  // Limit input to printable ASCII (0x20..0x7E)
  static constexpr int MAX_DATA = 48;
  uint8_t syms[MAX_DATA + 4];  // StartB + data + check + Stop
  int symCount = 0;

  syms[symCount++] = 104;  // Start B

  int checksum = 104;  // Start B value
  int dataCount = 0;

  for (char c : std::string_view(inputText, inputLength)) {
    if (c < 0x20 || c > 0x7E) continue;
    if (dataCount >= MAX_DATA) break;
    uint8_t codeIdx = static_cast<uint8_t>(c - 0x20);  // maps space->0, ! -> 1, etc.
    syms[symCount++] = codeIdx;
    checksum += static_cast<int>(codeIdx) * (dataCount + 1);
    dataCount++;
  }

  uint8_t checkVal = static_cast<uint8_t>(checksum % 103);
  syms[symCount++] = checkVal;
  syms[symCount++] = 106;  // Stop

  // Calculate total units:
  // Each normal symbol = 11 units; Stop = 13 units (6-element pattern = 11 + trailing bar 2)
  // Quiet zones: 10 units each side
  int totalUnits = 10 + 10;  // quiet zones
  for (int i = 0; i < symCount; i++) {
    if (syms[i] == 106) {
      totalUnits += 13;  // Stop: 11 + trailing bar 2
    } else {
      totalUnits += 11;
    }
  }

  const int availW = pageWidth - ml - mr - 20;  // 10px margin each side
  int unitW = availW / totalUnits;
  if (unitW < 1) {
    renderer.drawCenteredText(UI_10_FONT_ID, headerBottom + 40, tr(STR_BARCODE_TOO_WIDE));
    return;
  }

  // Center the barcode
  const int barcodeW = totalUnits * unitW;
  const int startX = ml + (pageWidth - ml - mr - barcodeW) / 2;

  // Bar height: fill most of the available vertical space
  const int barcodeH = std::max(20, std::min(220, footerTop - headerBottom - 70));
  const int barcodeY = headerBottom + (footerTop - headerBottom - barcodeH - 30) / 2;

  // Draw quiet zone (white — already clear), then symbols
  int cx = startX + 10 * unitW;  // skip left quiet zone

  for (int i = 0; i < symCount; i++) {
    const uint8_t* pat = CODE128_PATTERNS[syms[i]];
    drawPattern(cx, barcodeY, barcodeH, pat, unitW);

    // Advance past all 6 elements
    int symW = 0;
    for (int j = 0; j < 6; j++) symW += pat[j] * unitW;
    cx += symW;

    // Stop symbol: append trailing bar (width 2 units)
    if (syms[i] == 106) {
      renderer.fillRect(cx, barcodeY, 2 * unitW, barcodeH, true);
      cx += 2 * unitW;
    }
  }

  // Draw text label below barcode
  const int labelY = barcodeY + barcodeH + 10;
  renderer.drawCenteredText(UI_10_FONT_ID, labelY, inputText, true);
}

// --- Code 39 ---

void BarcodeActivity::renderCode39() const {
  int mt, mr, mb, ml;
  renderer.getOrientedViewableTRBL(&mt, &mr, &mb, &ml);
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  const int headerBottom = mt + metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int footerTop = pageHeight - mb - metrics.buttonHintsHeight - metrics.verticalSpacing;

  // Each character: 5 bars + 4 spaces = 9 elements, plus 1-unit inter-character gap
  // Narrow = 1 unit, Wide = 3 units
  // A typical character has: 2 wide + 7 narrow = 2*3 + 7*1 = 13 units (+ 1 gap = 14)
  // Star (start/stop): same pattern

  // Build list of pattern indices (into CODE39_PATTERNS / CODE39_STAR)
  static constexpr int MAX_C39 = 32;
  uint16_t patterns[MAX_C39 + 2];  // +2 for start/stop stars
  int count = 0;
  patterns[count++] = CODE39_STAR;

  for (char c : std::string_view(inputText, inputLength)) {
    if (count - 1 >= MAX_C39) break;  // leave room for stop
    // Uppercase
    char uc = (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
    // Find in character table
    const char* found = nullptr;
    for (int k = 0; CODE39_CHARS[k]; k++) {
      if (CODE39_CHARS[k] == uc) {
        found = CODE39_CHARS + k;
        patterns[count++] = CODE39_PATTERNS[k];
        break;
      }
    }
    (void)found;  // unsupported chars silently skipped
  }
  patterns[count++] = CODE39_STAR;

  // Calculate units per character: sum of narrow/wide elements
  // For each pattern, tally units
  auto charUnits = [](uint16_t pat) -> int {
    int u = 0;
    for (int b = 8; b >= 0; b--) {
      u += ((pat >> b) & 1) ? 3 : 1;
    }
    return u;
  };

  int totalUnits = 20;  // quiet zones
  for (int i = 0; i < count; i++) {
    totalUnits += charUnits(patterns[i]);
    if (i < count - 1) totalUnits += 1;  // inter-char gap
  }

  const int availW = pageWidth - ml - mr - 20;
  int unitW = availW / totalUnits;
  if (unitW < 1) {
    renderer.drawCenteredText(UI_10_FONT_ID, headerBottom + 40, tr(STR_BARCODE_TOO_WIDE));
    return;
  }

  const int barcodeW = totalUnits * unitW;
  const int startX = ml + (pageWidth - ml - mr - barcodeW) / 2;

  const int barcodeH = std::max(20, std::min(220, footerTop - headerBottom - 70));
  const int barcodeY = headerBottom + (footerTop - headerBottom - barcodeH - 30) / 2;

  int cx = startX + 10 * unitW;  // left quiet zone

  for (int i = 0; i < count; i++) {
    uint16_t pat = patterns[i];
    // Draw 9 elements (alternating bar/space, bar first)
    for (int b = 8; b >= 0; b--) {
      int w = ((pat >> b) & 1) ? (3 * unitW) : unitW;
      int elemIdx = 8 - b;  // 0-based element index
      if (elemIdx % 2 == 0) {
        // Bar
        renderer.fillRect(cx, barcodeY, w, barcodeH, true);
      }
      cx += w;
    }
    // Inter-character gap (1 narrow space)
    if (i < count - 1) {
      cx += unitW;
    }
  }

  // Label
  const int labelY = barcodeY + barcodeH + 10;
  renderer.drawCenteredText(UI_10_FONT_ID, labelY, inputText, true);
}

// --- EAN-13 ---

void BarcodeActivity::renderEan13() const {
  int mt, mr, mb, ml;
  renderer.getOrientedViewableTRBL(&mt, &mr, &mb, &ml);
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  const int headerBottom = mt + metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int footerTop = pageHeight - mb - metrics.buttonHintsHeight - metrics.verticalSpacing;

  // Validate: must be exactly 13 digits
  if (inputLength != 13) {
    renderer.drawCenteredText(UI_10_FONT_ID, (headerBottom + footerTop) / 2 - 20, tr(STR_BARCODE_EAN_LENGTH), true);
    return;
  }
  for (char c : std::string_view(inputText, inputLength)) {
    if (c < '0' || c > '9') {
      renderer.drawCenteredText(UI_10_FONT_ID, (headerBottom + footerTop) / 2 - 20, tr(STR_BARCODE_EAN_DIGITS), true);
      return;
    }
  }

  int digits[13];
  for (int i = 0; i < 13; i++) digits[i] = inputText[i] - '0';

  // Verify check digit (digit 12)
  int sum = 0;
  for (int i = 0; i < 12; i++) {
    sum += digits[i] * (i % 2 == 0 ? 1 : 3);
  }
  int calcCheck = (10 - (sum % 10)) % 10;
  if (calcCheck != digits[12]) {
    char errbuf[48];
    snprintf(errbuf, sizeof(errbuf), tr(STR_BARCODE_CHECK), calcCheck);
    renderer.drawCenteredText(UI_10_FONT_ID, (headerBottom + footerTop) / 2 - 20, errbuf, true);
    return;
  }

  // EAN-13 structure:
  //  3 (guard) | 6*7 (left, L/G by parity) | 5 (center guard) | 6*7 (right, R) | 3 (guard)
  // Total = 3 + 42 + 5 + 42 + 3 = 95 modules + 2 quiet zones (11 each)
  const int totalUnits = 11 + 3 + 42 + 5 + 42 + 3 + 11;  // = 117

  const int availW = pageWidth - ml - mr - 20;
  int unitW = availW / totalUnits;
  if (unitW < 1) {
    renderer.drawCenteredText(UI_10_FONT_ID, headerBottom + 40, tr(STR_BARCODE_TOO_WIDE));
    return;
  }

  const int barcodeW = totalUnits * unitW;
  const int startX = ml + (pageWidth - ml - mr - barcodeW) / 2;

  const int barcodeH = std::max(20, std::min(220, footerTop - headerBottom - 70));
  const int guardExt = 15;  // guard bars extend below normal bars
  const int barcodeY = headerBottom + (footerTop - headerBottom - barcodeH - 30) / 2;

  // Helper: draw a 7-module pattern from a uint8_t bitmask (MSB first)
  auto draw7 = [&](int& cx, uint8_t pattern, bool extended) {
    int h = extended ? (barcodeH + guardExt) : barcodeH;
    for (int bit = 6; bit >= 0; bit--) {
      int w = unitW;
      if ((pattern >> bit) & 1) {
        renderer.fillRect(cx, barcodeY, w, h, true);
      }
      cx += w;
    }
  };

  // Helper: draw guard bars (bar-space-bar = 1,0,1)
  auto drawGuard = [&](int& cx, bool extended) {
    int h = extended ? (barcodeH + guardExt) : barcodeH;
    renderer.fillRect(cx, barcodeY, unitW, h, true);
    cx += unitW;  // bar
    cx += unitW;  // space
    renderer.fillRect(cx, barcodeY, unitW, h, true);
    cx += unitW;  // bar
  };

  // Helper: draw center guard (space-bar-space-bar-space = 0,1,0,1,0)
  auto drawCenter = [&](int& cx) {
    cx += unitW;  // space
    renderer.fillRect(cx, barcodeY, unitW, barcodeH + guardExt, true);
    cx += unitW;  // bar
    cx += unitW;  // space
    renderer.fillRect(cx, barcodeY, unitW, barcodeH + guardExt, true);
    cx += unitW;  // bar
    cx += unitW;  // space
  };

  int cx = startX + 11 * unitW;  // left quiet zone

  // Left guard
  drawGuard(cx, true);

  // Left 6 digits using L/G parity from first digit
  uint8_t parity = EAN13_PARITY[digits[0]];
  for (int i = 1; i <= 6; i++) {
    int d = digits[i];
    bool useG = (parity >> (6 - i)) & 1;
    draw7(cx, useG ? EAN13_G[d] : EAN13_L[d], false);
  }

  // Center guard
  drawCenter(cx);

  // Right 6 digits using R-codes
  for (int i = 7; i <= 12; i++) {
    draw7(cx, EAN13_R[digits[i]], false);
  }

  // Right guard
  drawGuard(cx, true);

  // Draw the first digit (system digit) to the left of the barcode
  char sysBuf[2] = {static_cast<char>('0' + digits[0]), '\0'};
  int sysDX = startX + 11 * unitW - renderer.getTextWidth(UI_10_FONT_ID, sysBuf) - 4;
  renderer.drawText(UI_10_FONT_ID, sysDX, barcodeY + barcodeH - 16, sysBuf, true);

  // Draw the 12-digit label split below guard extensions
  char leftBuf[8], rightBuf[8];
  snprintf(leftBuf, sizeof(leftBuf), "%d%d%d%d%d%d", digits[1], digits[2], digits[3], digits[4], digits[5], digits[6]);
  snprintf(rightBuf, sizeof(rightBuf), "%d%d%d%d%d%d", digits[7], digits[8], digits[9], digits[10], digits[11],
           digits[12]);

  const int labelY = barcodeY + barcodeH + guardExt + 4;

  // Left group: center under left 6 digits zone
  // Left zone: after guard (3) for 42 modules
  int leftZoneX = startX + (11 + 3) * unitW;
  int leftZoneW = 42 * unitW;
  int rightZoneX = startX + (11 + 3 + 42 + 5) * unitW;
  int rightZoneW = 42 * unitW;

  int lw = renderer.getTextWidth(UI_10_FONT_ID, leftBuf);
  int rw = renderer.getTextWidth(UI_10_FONT_ID, rightBuf);
  renderer.drawText(UI_10_FONT_ID, leftZoneX + (leftZoneW - lw) / 2, labelY, leftBuf, true);
  renderer.drawText(UI_10_FONT_ID, rightZoneX + (rightZoneW - rw) / 2, labelY, rightBuf, true);
}
