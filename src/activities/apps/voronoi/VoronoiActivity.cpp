#include "VoronoiActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

uint32_t VoronoiActivity::randomValue() {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return rng;
}
void VoronoiActivity::generate() {
  const Rect area = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  width = std::max(0, area.width);
  height = std::max(0, area.height - 2 * renderer.getLineHeight(UI_10_FONT_ID));
  step = std::max(8, std::max((width + 99) / 100, (height + 59) / 60));
  cols = (width + step - 1) / step;
  rows = (height + step - 1) / step;
  if (!width || !height) return;
  for (int i = 0; i < count; ++i) {
    points[i] = {int(randomValue() % width), int(randomValue() % height)};
  }
  // At most 6,000 cells x 40 points; recomputed only on explicit input.
  for (int y = 0; y < rows; ++y)
    for (int x = 0; x < cols; ++x) {
      const int px = std::min(x * step + step / 2, width - 1);
      const int py = std::min(y * step + step / 2, height - 1);
      int64_t best = INT64_MAX;
      for (int i = 0; i < count; ++i) {
        const int64_t dx = px - points[i].x, dy = py - points[i].y;
        const int64_t distance = dx * dx + dy * dy;
        if (distance < best) {
          best = distance;
          nearest[y][x] = i;
        }
      }
    }
}
void VoronoiActivity::onEnter() {
  Activity::onEnter();
  rng = millis() | 1u;
  generate();
  requestUpdate();
}
void VoronoiActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  const bool more = mappedInput.wasPressed(MappedInputManager::Button::Right);
  const bool less = mappedInput.wasPressed(MappedInputManager::Button::Left);
  if (more || less || mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    RenderLock lock(*this);
    count = std::max(5, std::min(40, count + (more ? 5 : less ? -5 : 0)));
    generate();
    requestUpdate();
  }
}
void VoronoiActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect area = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  for (int y = 0; y < rows; ++y)
    for (int x = 0; x < cols; ++x) {
      const uint8_t id = nearest[y][x];
      const int w = std::min(step, width - x * step), h = std::min(step, height - y * step);
      for (int dy = 0; dy < h; ++dy)
        for (int dx = 0; dx < w; ++dx) {
          const int px = x * step + dx, py = y * step + dy;
          if ((px + py * 3) % 4 < id % 4) renderer.drawPixel(area.x + px, area.y + py, true);
        }
      if (x + 1 < cols && id != nearest[y][x + 1])
        renderer.fillRect(area.x + (x + 1) * step - 1, area.y + y * step, 1, h, true);
      if (y + 1 < rows && id != nearest[y + 1][x])
        renderer.fillRect(area.x + x * step, area.y + (y + 1) * step - 1, w, 1, true);
    }
  if (width && height)
    for (int i = 0; i < count; ++i) {
      const int x = points[i].x, y = points[i].y;
      renderer.fillRect(area.x + x, area.y + y, std::min(3, width - x), std::min(3, height - y), (i % 4) < 2);
    }
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_VORONOI), false);
  else
    GUI.drawHeader(renderer, header, tr(STR_APP_VORONOI));
  char text[48];
  snprintf(text, sizeof(text), tr(STR_VORONOI_COUNT), count);
  UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID, area.y + height, text);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONFIRM), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
