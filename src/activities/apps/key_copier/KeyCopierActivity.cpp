// Key-type ranges adapted from biscuit, MIT, Copyright (c) 2025 Dave Allie.
// The approved scope is reference charts only: no capture, import or key files.
#include "KeyCopierActivity.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "components/UITheme.h"
#include "fontIds.h"

namespace {
struct KeyType {
  StrId title;
  uint8_t cuts, minDepth, maxDepth;
};
static constexpr KeyType TYPES[] = {
    {StrId::STR_KEY_KW1, 5, 1, 7},     {StrId::STR_KEY_SC1, 5, 0, 9},     {StrId::STR_KEY_Y1, 5, 1, 9},
    {StrId::STR_KEY_CUSTOM6, 6, 0, 9}, {StrId::STR_KEY_CUSTOM7, 7, 0, 9},
};
}  // namespace
void KeyCopierActivity::onEnter() {
  Activity::onEnter();
  typeIndex = 0;
  requestUpdate();
}
void KeyCopierActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
      mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    typeIndex = (typeIndex + TYPE_COUNT - 1) % TYPE_COUNT;
    requestUpdate();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down) ||
      mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    typeIndex = (typeIndex + 1) % TYPE_COUNT;
    requestUpdate();
  }
}
void KeyCopierActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto& key = TYPES[typeIndex];
  int mt, mr, mb, ml;
  renderer.getOrientedViewableTRBL(&mt, &mr, &mb, &ml);
  const int width = renderer.getScreenWidth() - ml - mr;
  const int top = mt + metrics.topPadding;
  GUI.drawHeader(renderer, Rect{ml, top, width, metrics.headerHeight}, tr(STR_APP_KEY_CHARTS));
  int y = top + metrics.headerHeight + 8;
  renderer.drawCenteredText(UI_10_FONT_ID, y, I18N.get(key.title));
  y += renderer.getLineHeight(UI_10_FONT_ID) + 8;
  char info[64];
  snprintf(info, sizeof(info), tr(STR_KEY_RANGE), key.cuts, key.minDepth, key.maxDepth);
  renderer.drawCenteredText(SMALL_FONT_ID, y, info);
  y += renderer.getLineHeight(SMALL_FONT_ID) + 8;
  renderer.drawCenteredText(SMALL_FONT_ID, y, tr(STR_KEY_SCHEMATIC));
  y += renderer.getLineHeight(SMALL_FONT_ID) + 12;
  const int bottom = renderer.getScreenHeight() - mb - metrics.buttonHintsHeight - 8;
  const int count = key.maxDepth - key.minDepth + 1;
  const int step = std::max(1, (bottom - y) / count);
  const int left = ml + metrics.contentSidePadding + 35;
  const int chartWidth = std::max(1, width - 2 * metrics.contentSidePadding - 45);
  for (int i = 0; i < count; ++i) {
    char depth[4];
    snprintf(depth, sizeof(depth), "%d", key.minDepth + i);
    const int rowY = y + i * step;
    renderer.drawText(SMALL_FONT_ID, ml + metrics.contentSidePadding, rowY, depth);
    // Equal spacing depicts ordinal codes only; no dimensions or cutting profile.
    renderer.drawLine(left, rowY + 4, left + chartWidth, rowY + 4);
    for (int cut = 0; cut < key.cuts; ++cut) {
      const int x = left + (cut + 1) * chartWidth / (key.cuts + 1);
      renderer.drawLine(x, rowY, x, rowY + 8);
    }
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_KEY_PREVIOUS), tr(STR_KEY_NEXT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
