#include "AppLauncherActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <vector>

#include "I18nKeys.h"
#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "components/UIThemeTokens.h"
#include "components/UiAppHelpers.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {
constexpr fui::ActionId ACTION_ROW = 1;
constexpr const char* TAG = "APPS";
}  // namespace

AppLauncherActivity::AppLauncherActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                         const std::optional<AppCategory> category)
    : Activity("AppLauncher", renderer, mappedInput),
      category(category),
      uiTarget(makeUiTarget(renderer)),
      app(uiTarget, uiTarget.deviceContext()) {}

int AppLauncherActivity::itemCount() const {
  if (!category.has_value()) return static_cast<int>(kAppCategoryCount);
  return static_cast<int>(appCountInCategory(*category));
}

const char* AppLauncherActivity::title() const {
  if (!category.has_value()) return tr(STR_APP_LAUNCHER_TITLE);
  size_t count = 0;
  const AppCategoryInfo* categories = appCategories(&count);
  for (size_t i = 0; i < count; ++i) {
    if (categories[i].category == *category) return I18N.get(categories[i].title);
  }
  return tr(STR_APP_LAUNCHER_TITLE);
}

void AppLauncherActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  topIndex = 0;
  visibleRows = 1;
  initialViewportPending = true;
  uiReady = false;
  app.setTheme(uiThemeTokens(uiTarget));
  app.on(ACTION_ROW, &AppLauncherActivity::onRowEvent, this);
  app.setScreen(&AppLauncherActivity::launcherScreen, this);
  requestUpdate();
}

void AppLauncherActivity::onExit() { Activity::onExit(); }

void AppLauncherActivity::openCategory(const AppCategory next) {
  auto activity = makeUniqueNoThrow<AppLauncherActivity>(renderer, mappedInput, std::optional<AppCategory>(next));
  if (!activity) {
    LOG_ERR(TAG, "OOM opening category %d", static_cast<int>(next));
    return;
  }
  startActivityForResult(std::move(activity), [](const ActivityResult&) {});
}

void AppLauncherActivity::openApp(const AppEntry& entry) {
  if (entry.create == nullptr) return;
  auto activity = entry.create(renderer, mappedInput);
  if (!activity) {
    // Rule 6/16: an app that will not fit is reported, not crashed into.
    LOG_ERR(TAG, "OOM launching %s", I18N.get(entry.title));
    return;
  }
  startActivityForResult(std::move(activity), [](const ActivityResult&) {});
}

void AppLauncherActivity::handleSelection() {
  const int count = itemCount();
  if (count == 0 || selectedIndex < 0 || selectedIndex >= count) return;

  if (!category.has_value()) {
    size_t categoryCount = 0;
    const AppCategoryInfo* categories = appCategories(&categoryCount);
    if (static_cast<size_t>(selectedIndex) >= categoryCount) return;
    openCategory(categories[selectedIndex].category);
    return;
  }

  size_t appCount = 0;
  const AppEntry* apps = appsInCategory(*category, &appCount);
  if (apps == nullptr || static_cast<size_t>(selectedIndex) >= appCount) return;
  openApp(apps[selectedIndex]);
}

void AppLauncherActivity::onRowEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<AppLauncherActivity*>(user);
  if (event.value < 0 || event.value >= self->itemCount()) return;
  self->selectedIndex = event.value;
  self->app.clearTapFlash();
  self->handleSelection();
}

void AppLauncherActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer)) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  const int count = itemCount();

  if (uiReady) {
    const fui::InputSnapshot snap = touchSnapshotFrom(mappedInput);
    if (snap.touchPressed || snap.touchReleased) {
      const auto event = app.route(snap);
      if (app.invalidated()) requestUpdate();
      if (event) return;
    }
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }
  if (count == 0) return;

  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
    const int next = scrollListBy(topIndex, swipe == MappedInputManager::SwipeDir::Up ? visibleRows : -visibleRows,
                                  visibleRows, count);
    if (next != topIndex) {
      topIndex = next;
      requestUpdate();
    }
    return;
  }

  const auto move = [this, count](const int index) {
    selectedIndex = index;
    topIndex = followListSelection(selectedIndex, topIndex, visibleRows, count);
    requestUpdate();
  };
  buttonNavigator.onNextRelease([this, &move, count] { move(ButtonNavigator::nextIndex(selectedIndex, count)); });
  buttonNavigator.onPreviousRelease(
      [this, &move, count] { move(ButtonNavigator::previousIndex(selectedIndex, count)); });
  buttonNavigator.onNextContinuous(
      [this, &move, count] { move(ButtonNavigator::nextPageIndex(selectedIndex, count, visibleRows)); });
  buttonNavigator.onPreviousContinuous(
      [this, &move, count] { move(ButtonNavigator::previousPageIndex(selectedIndex, count, visibleRows)); });
}

void AppLauncherActivity::launcherScreen(UiApp::ScreenType& screen, void* user) {
  static_cast<AppLauncherActivity*>(user)->buildLauncherScreen(screen);
}

void AppLauncherActivity::buildLauncherScreen(UiApp::ScreenType& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput)), 0,
                  static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  const int count = itemCount();
  if (count == 0) {
    // An empty tile says so rather than showing a blank list.
    screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
    return;
  }

  std::vector<fui::ListItem> items;
  items.reserve(static_cast<size_t>(count));  // rule 2: reserve before the push loop

  // Category rows carry their app count as the row value, so an empty tile is
  // visible before you open it.
  char countLabels[kAppCategoryCount][8] = {};

  if (!category.has_value()) {
    size_t categoryCount = 0;
    const AppCategoryInfo* categories = appCategories(&categoryCount);
    for (size_t i = 0; i < categoryCount; ++i) {
      fui::ListItem item;
      item.label = I18N.get(categories[i].title);
      snprintf(countLabels[i], sizeof(countLabels[i]), "%u",
               static_cast<unsigned>(appCountInCategory(categories[i].category)));
      item.value = countLabels[i];
      item.actionValue = static_cast<int16_t>(i);
      items.push_back(item);
    }
  } else {
    size_t appCount = 0;
    const AppEntry* apps = appsInCategory(*category, &appCount);
    for (size_t i = 0; i < appCount; ++i) {
      fui::ListItem item;
      item.label = I18N.get(apps[i].title);
      item.actionValue = static_cast<int16_t>(i);
      items.push_back(item);
    }
  }

  fui::ListProps props;
  props.items = items.data();
  props.count = static_cast<uint16_t>(items.size());
  props.selectedIndex = static_cast<int16_t>(selectedIndex);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().bodyText;
  const auto rows = configureUiList(props, screen.theme(), screen.body());
  visibleRows = rows > 0 ? rows : 1;
  topIndex = initialViewportPending ? followListSelection(selectedIndex, 0, visibleRows, count)
                                    : scrollListBy(topIndex, 0, visibleRows, count);
  initialViewportPending = false;
  props.topIndex = static_cast<uint16_t>(topIndex);
  screen.list(props);
}

void AppLauncherActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, uiTarget, header, title(), false);
  } else {
    GUI.drawHeader(renderer, header, title());
  }

  uiReady = false;
  app.render();
  uiReady = true;

  if (itemCount() == 0) {
    const auto pageHeight = renderer.getScreenHeight();
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_APP_CATEGORY_EMPTY), true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
