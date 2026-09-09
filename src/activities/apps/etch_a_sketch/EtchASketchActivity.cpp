// Adapted from biscuit, MIT, Copyright (c) 2025 Dave Allie.
#include "EtchASketchActivity.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "components/UITheme.h"
#include "fontIds.h"

void EtchASketchActivity::onEnter() {
  Activity::onEnter();
  canvas = makeUniqueNoThrow<uint8_t[]>(CANVAS_BYTES);
  if (!canvas) {
    LOG_ERR("EtchASketch", "Cannot allocate %u-byte drawing", static_cast<unsigned>(CANVAS_BYTES));
    finish();
    return;
  }
  memset(canvas.get(), 0, CANVAS_BYTES);
  const bool landscape = renderer.getScreenWidth() > renderer.getScreenHeight();
  canvasW = landscape ? 240 : 160;
  canvasH = landscape ? 160 : 240;
  cursorX = canvasW / 2;
  cursorY = canvasH / 2;
  penDown = true;
  saveStatus = SaveStatus::None;
  lastMove = millis();
  requestUpdate();
}

void EtchASketchActivity::onExit() {
  canvas.reset();
  Activity::onExit();
}

void EtchASketchActivity::setPixel(int x, int y) {
  if (!canvas || x < 0 || y < 0 || x >= canvasW || y >= canvasH) return;
  const int bit = y * canvasW + x;
  canvas[bit / 8] |= 1u << (bit % 8);
}

bool EtchASketchActivity::getPixel(int x, int y) const {
  if (!canvas || x < 0 || y < 0 || x >= canvasW || y >= canvasH) return false;
  const int bit = y * canvasW + x;
  return (canvas[bit / 8] >> (bit % 8)) & 1;
}

void EtchASketchActivity::saveToBmp() {
  saveStatus = SaveStatus::Failed;
  if (!Storage.exists("/crossink/drawings") && !Storage.mkdir("/crossink/drawings", true)) {
    LOG_ERR("EtchASketch", "Cannot create drawings directory");
    return;
  }
  // Exclusive creation avoids overwriting an earlier drawing after a reboot.
  char filename[72];
  HalFile file;
  for (unsigned attempt = 0; attempt < 32; ++attempt) {
    snprintf(filename, sizeof(filename), "/crossink/drawings/sketch_%lu_%u.bmp", millis(), attempt);
    if (Storage.exists(filename)) continue;
    file = Storage.open(filename, O_WRITE | O_CREAT | O_EXCL);
    break;
  }
  if (!file) {
    LOG_ERR("EtchASketch", "Cannot create drawing file");
    return;
  }
  const int rowBytes = ((canvasW + 31) / 32) * 4;
  const uint32_t imageSize = rowBytes * canvasH;
  uint8_t header[62] = {};
  auto put32 = [&header](int offset, uint32_t value) {
    for (int i = 0; i < 4; ++i) header[offset + i] = static_cast<uint8_t>(value >> (8 * i));
  };
  header[0] = 'B';
  header[1] = 'M';
  put32(2, 62 + imageSize);
  header[10] = 62;
  header[14] = 40;
  put32(18, canvasW);
  put32(22, canvasH);
  header[26] = 1;
  header[28] = 1;
  put32(34, imageSize);
  header[54] = header[55] = header[56] = 255;  // white at index zero
  bool ok = file.write(header, sizeof(header)) == sizeof(header);
  uint8_t row[32];  // maximum 240 pixels, padded to four bytes; no row heap allocation
  for (int y = canvasH - 1; ok && y >= 0; --y) {
    memset(row, 0, sizeof(row));
    for (int x = 0; x < canvasW; ++x)
      if (getPixel(x, y)) row[x / 8] |= 0x80u >> (x % 8);
    ok = file.write(row, rowBytes) == static_cast<size_t>(rowBytes);
  }
  if (ok) ok = file.sync();
  const bool closed = file.close();
  if (!ok || !closed) {
    LOG_ERR("EtchASketch", "Drawing write/sync/close failed: %s", filename);
    // Only remove the incomplete file created exclusively by this save.
    if (!Storage.remove(filename)) LOG_ERR("EtchASketch", "Cannot remove incomplete drawing: %s", filename);
    return;
  }
  saveStatus = SaveStatus::Saved;
  LOG_INF("EtchASketch", "Saved %s", filename);
}

void EtchASketchActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (!canvas) return;
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    saveToBmp();
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
    penDown = !penDown;
    saveStatus = SaveStatus::None;
    requestUpdate();
    return;
  }
  const unsigned long now = millis();
  if (now - lastMove < 80) return;
  lastMove = now;
  const int oldX = cursorX, oldY = cursorY;
  if (mappedInput.isPressed(MappedInputManager::Button::Up)) cursorY = std::max(0, cursorY - 1);
  if (mappedInput.isPressed(MappedInputManager::Button::Down)) cursorY = std::min(canvasH - 1, cursorY + 1);
  if (mappedInput.isPressed(MappedInputManager::Button::Left)) cursorX = std::max(0, cursorX - 1);
  if (mappedInput.isPressed(MappedInputManager::Button::Right)) cursorX = std::min(canvasW - 1, cursorX + 1);
  if (oldX != cursorX || oldY != cursorY) {
    if (penDown) setPixel(cursorX, cursorY);
    saveStatus = SaveStatus::None;
    requestUpdate();
  }
}

void EtchASketchActivity::render(RenderLock&&) {
  renderer.clearScreen();
  if (!canvas) {
    renderer.displayBuffer();
    return;
  }
  const auto& metrics = UITheme::getInstance().getMetrics();
  int mt, mr, mb, ml;
  renderer.getOrientedViewableTRBL(&mt, &mr, &mb, &ml);
  const int width = renderer.getScreenWidth() - ml - mr;
  const int top = mt + metrics.topPadding;
  GUI.drawHeader(renderer, Rect{ml, top, width, metrics.headerHeight}, tr(STR_APP_ETCH));
  const int boardTop = top + metrics.headerHeight + 35;
  const int availableH = renderer.getScreenHeight() - mb - metrics.buttonHintsHeight - boardTop - 8;
  const int scale = std::max(1, std::min((width - 8) / canvasW, availableH / canvasH));
  const int ox = ml + (width - canvasW * scale) / 2;
  const int oy = boardTop + (availableH - canvasH * scale) / 2;
  renderer.drawRect(ox - 1, oy - 1, canvasW * scale + 2, canvasH * scale + 2);
  for (int y = 0; y < canvasH; ++y)
    for (int x = 0; x < canvasW; ++x) {
      if (getPixel(x, y)) renderer.fillRect(ox + x * scale, oy + y * scale, scale, scale);
    }
  // Cursor overlay is never stored or exported in the drawing.
  for (int i = -3; i <= 3; ++i) {
    const int x = cursorX + i, y = cursorY + i;
    if (x >= 0 && x < canvasW)
      renderer.fillRect(ox + x * scale, oy + cursorY * scale, scale, scale, !getPixel(x, cursorY));
    if (y >= 0 && y < canvasH)
      renderer.fillRect(ox + cursorX * scale, oy + y * scale, scale, scale, !getPixel(cursorX, y));
  }
  const char* status = saveStatus == SaveStatus::Saved    ? tr(STR_ETCH_SAVED)
                       : saveStatus == SaveStatus::Failed ? tr(STR_ETCH_SAVE_FAILED)
                       : penDown                          ? tr(STR_ETCH_PEN_DOWN)
                                                          : tr(STR_ETCH_PEN_UP);
  renderer.drawCenteredText(SMALL_FONT_ID, top + metrics.headerHeight + 5, status);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_ETCH_SAVE), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
