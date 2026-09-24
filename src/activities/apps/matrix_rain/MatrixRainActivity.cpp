#include "MatrixRainActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

constexpr unsigned long MatrixRainActivity::SPEED_INTERVALS[];

char MatrixRainActivity::randomChar() {
  static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%&*<>{}|/=+-";
  return charset[randomValue() % (sizeof(charset) - 1)];
}

void MatrixRainActivity::spawnDrop(int col) {
  dropActive[col] = true;
  dropHead[col] = -(int)(randomValue() % (rows / 2 + 1));
  dropLength[col] = 4 + (randomValue() % 12);
  dropSpeed[col] = 1 + (randomValue() % 2);
}

void MatrixRainActivity::initColumns() {
  const Rect area = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  originX = area.x;
  originY = area.y;
  cols = area.width / CHAR_W;
  rows = (area.height - 2 * renderer.getLineHeight(UI_10_FONT_ID)) / CHAR_H;
  if (rows < 0) rows = 0;
  if (cols > MAX_COLS) cols = MAX_COLS;
  if (rows > MAX_ROWS) rows = MAX_ROWS;

  for (int i = 0; i < rows * cols; i++) grid[i] = randomChar();

  for (int c = 0; c < cols; c++) {
    dropActive[c] = (randomValue() % 3) != 0;
    dropHead[c] = -(int)(randomValue() % (rows / 2 + 1));
    dropLength[c] = 4 + (randomValue() % 12);
    dropSpeed[c] = 1 + (randomValue() % 2);
  }
}

void MatrixRainActivity::advanceFrame() {
  for (int c = 0; c < cols; c++) {
    if (!dropActive[c]) {
      int spawnChance = (density == 0) ? 8 : (density == 1) ? 4 : 2;
      if ((int)(randomValue() % (spawnChance * 10)) == 0) spawnDrop(c);
      continue;
    }

    if (frameCount % dropSpeed[c] != 0) continue;

    dropHead[c]++;

    if (dropHead[c] >= 0 && dropHead[c] < rows) {
      grid[dropHead[c] * cols + c] = randomChar();
    }

    // Flicker effect: randomly change a char in the trail
    int flickerRow = dropHead[c] - (int)(randomValue() % (dropLength[c] + 1));
    if (flickerRow >= 0 && flickerRow < rows) {
      grid[flickerRow * cols + c] = randomChar();
    }

    if (dropHead[c] - dropLength[c] >= rows) {
      dropActive[c] = false;
    }
  }
  frameCount++;
}

void MatrixRainActivity::drawChar(int col, int row, char c, int intensity) {
  int x = originX + col * CHAR_W;
  int y = originY + row * CHAR_H;
  char buf[2] = {c, '\0'};

  switch (intensity) {
    case 4:  // HEAD — bold (draw twice offset by 1px)
      renderer.drawText(SMALL_FONT_ID, x, y, buf, true);
      renderer.drawText(SMALL_FONT_ID, x + 1, y, buf, true);
      break;
    case 3:  // BRIGHT — normal black character
      renderer.drawText(SMALL_FONT_ID, x, y, buf, true);
      break;
    case 2:  // DIM — draw char then punch 50% checkerboard white
      renderer.drawText(SMALL_FONT_ID, x, y, buf, true);
      for (int dy = 0; dy < CHAR_H; dy++)
        for (int dx = (dy % 2); dx < CHAR_W; dx += 2) renderer.drawPixel(x + dx, y + dy, false);
      break;
    case 1:  // FADING — draw char then keep only 25% of pixels
      renderer.drawText(SMALL_FONT_ID, x, y, buf, true);
      for (int dy = 0; dy < CHAR_H; dy++)
        for (int dx = 0; dx < CHAR_W; dx++)
          if (!((dx + dy * 3) % 4 == 0)) renderer.drawPixel(x + dx, y + dy, false);
      break;
    case 0:
    default:
      break;
  }
}

void MatrixRainActivity::onEnter() {
  Activity::onEnter();
  rng = millis() | 1u;
  initColumns();
  lastFrameMs = millis();
  frameCount = 0;
  speedLevel = 1;
  density = 2;
  requestUpdate();
}

void MatrixRainActivity::onExit() { Activity::onExit(); }

void MatrixRainActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  RenderLock lock(*this);
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    paused = !paused;
    lastFrameMs = millis();
    requestUpdate();
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    if (speedLevel < 2) {
      speedLevel++;
      requestUpdate();
    }
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    if (speedLevel > 0) {
      speedLevel--;
      requestUpdate();
    }
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    if (density < 2) {
      density++;
      requestUpdate();
    }
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    if (density > 0) {
      density--;
      requestUpdate();
    }
  }

  unsigned long now = millis();
  if (!paused && now - lastFrameMs >= SPEED_INTERVALS[speedLevel]) {
    lastFrameMs = now;
    advanceFrame();
    requestUpdate();
  }
}

void MatrixRainActivity::render(RenderLock&&) {
  renderer.clearScreen();

  for (int c = 0; c < cols; c++) {
    if (!dropActive[c]) continue;

    for (int dist = 0; dist <= dropLength[c]; dist++) {
      int row = dropHead[c] - dist;
      if (row < 0 || row >= rows) continue;

      char ch = grid[row * cols + c];
      int intensity;
      if (dist == 0)
        intensity = 4;
      else if (dist <= 2)
        intensity = 3;
      else if (dist <= dropLength[c] / 2)
        intensity = 2;
      else
        intensity = 1;

      drawChar(c, row, ch, intensity);
    }
  }

  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_MATRIX_RAIN), false);
  else
    GUI.drawHeader(renderer, header, tr(STR_APP_MATRIX_RAIN));
  const Rect area = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  char status[96];
  snprintf(status, sizeof(status), tr(STR_MATRIX_STATE), unsigned(SPEED_INTERVALS[speedLevel]), density + 1);
  UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID,
                            area.y + area.height - 2 * renderer.getLineHeight(UI_10_FONT_ID), status);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), paused ? tr(STR_MONITOR_RESUME) : tr(STR_MONITOR_PAUSE),
                                            tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
