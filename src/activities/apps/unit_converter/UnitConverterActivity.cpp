// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
#include "UnitConverterActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

void UnitConverterActivity::onEnter() {
  Activity::onEnter();
  state = State::PickCategory;
  categoryIndex = 0;
  unitIndex = 0;
  selectedIndex = 0;
  inputValue = 1.0;
  snprintf(inputText, sizeof(inputText), "1");
  rowCount = 0;
  requestUpdate();
}

void UnitConverterActivity::onExit() { Activity::onExit(); }

size_t UnitConverterActivity::itemCount() const {
  size_t categoryCount = 0;
  const units::Category* cats = units::categories(&categoryCount);
  if (state == State::PickCategory) return categoryCount;
  if (categoryIndex >= categoryCount) return 0;
  return cats[categoryIndex].unitCount;
}

void UnitConverterActivity::computeRows() {
  rowCount = 0;
  size_t categoryCount = 0;
  const units::Category* cats = units::categories(&categoryCount);
  if (categoryIndex >= categoryCount) return;
  const units::Category& category = cats[categoryIndex];

  const size_t count = category.unitCount < kMaxUnits ? category.unitCount : kMaxUnits;
  for (size_t i = 0; i < count; ++i) {
    double converted = 0.0;
    if (!units::convert(categoryIndex, unitIndex, i, inputValue, &converted)) continue;
    char valueText[24];
    units::format(converted, valueText, sizeof(valueText));
    snprintf(rows[rowCount], kRowTextLength, "%s %s", valueText, category.units[i].abbreviation);
    ++rowCount;
  }
}

void UnitConverterActivity::promptForValue() {
  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_CONVERT_ENTER_VALUE),
                                                                 std::string(inputText), sizeof(inputText) - 1),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             requestUpdate();
                             return;
                           }
                           const auto* keyboard = std::get_if<KeyboardResult>(&result.data);
                           if (keyboard == nullptr) {
                             requestUpdate();
                             return;
                           }
                           snprintf(inputText, sizeof(inputText), "%s", keyboard->text.c_str());
                           inputValue = std::atof(inputText);
                           computeRows();
                           requestUpdate();
                         });
}

void UnitConverterActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    // Back walks the flow instead of leaving from the middle of it.
    if (state == State::ShowResults) {
      state = State::PickUnit;
      selectedIndex = static_cast<int>(unitIndex);
      requestUpdate();
      return;
    }
    if (state == State::PickUnit) {
      state = State::PickCategory;
      selectedIndex = static_cast<int>(categoryIndex);
      requestUpdate();
      return;
    }
    finish();
    return;
  }

  if (state == State::ShowResults) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) promptForValue();
    return;
  }

  const int count = static_cast<int>(itemCount());
  if (count == 0) return;

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (state == State::PickCategory) {
      categoryIndex = static_cast<size_t>(selectedIndex);
      unitIndex = 0;
      selectedIndex = 0;
      state = State::PickUnit;
    } else {
      unitIndex = static_cast<size_t>(selectedIndex);
      computeRows();
      state = State::ShowResults;
    }
    requestUpdate();
    return;
  }

  const auto move = [this, count](const int index) {
    selectedIndex = index;
    requestUpdate();
  };
  buttonNavigator.onNextRelease([this, &move, count] { move(ButtonNavigator::nextIndex(selectedIndex, count)); });
  buttonNavigator.onPreviousRelease(
      [this, &move, count] { move(ButtonNavigator::previousIndex(selectedIndex, count)); });
}

void UnitConverterActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  size_t categoryCount = 0;
  const units::Category* cats = units::categories(&categoryCount);

  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  const char* title = tr(STR_APP_UNIT_CONVERTER);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, header, title, false);
  } else {
    GUI.drawHeader(renderer, header, title);
  }

  const Rect content{0, header.y + header.height + metrics.verticalSpacing, pageWidth,
                     pageHeight - (header.y + header.height) - metrics.verticalSpacing - metrics.buttonHintsHeight};

  if (state == State::PickCategory) {
    GUI.drawList(renderer, content, static_cast<int>(categoryCount), selectedIndex,
                 [cats](const int index) { return std::string(cats[index].name); });
  } else if (state == State::PickUnit) {
    const units::Category& category = cats[categoryIndex];
    GUI.drawList(renderer, content, static_cast<int>(category.unitCount), selectedIndex,
                 [&category](const int index) { return std::string(category.units[index].name); });
  } else {
    const units::Category& category = cats[categoryIndex];
    char heading[48];
    snprintf(heading, sizeof(heading), "%s %s", inputText, category.units[unitIndex].abbreviation);
    renderer.drawCenteredText(UI_12_FONT_ID, content.y, heading, true, EpdFontFamily::BOLD);

    const auto* rowsPtr = rows;
    GUI.drawList(renderer, Rect{content.x, content.y + 30, content.width, content.height - 30},
                 static_cast<int>(rowCount), -1, [rowsPtr](const int index) { return std::string(rowsPtr[index]); });
  }

  const auto labels = state == State::ShowResults
                          ? mappedInput.mapLabels(tr(STR_BACK), tr(STR_CONVERT_VALUE), "", "")
                          : mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
