#ifdef SIMULATOR

#include "SimulatorSmokeTest.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <memory>
#include <vector>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "SimulatorCssTest.h"
#include "SimulatorGameOfLifeTest.h"
#include "activities/ActivityManager.h"
#include "activities/apps/AppLauncherActivity.h"
#include "activities/apps/AppRegistry.h"
#include "activities/reader/EpubReaderMenuActivity.h"
#include "activities/reader/ReaderOptionsActivity.h"
#include "components/UITheme.h"
#include "components/UIThemeTokens.h"
#include "components/UiAppHelpers.h"
#include "simulator/SimulatorHomeKeyInput.h"

extern ActivityManager activityManager;
extern GfxRenderer renderer;
extern MappedInputManager mappedInputManager;

namespace {

enum class SmokeStep : uint8_t {
  Start,
  Home,
  FileBrowser,
  RecentBooks,
  Settings,
  AppLauncher,
  AppCategoryList,
  AppScreens,
  QrInput,
  CipherInput,
  BasicAppInput,
  ReaderOptions,
  ReaderMenu,
  Sleep,
  Reader,
  ReaderInput,
  Done,
};

class SimulatorSmokeTest {
 public:
  void tick() {
    if (!enabled()) return;

    try {
      tickImpl();
    } catch (const std::exception& e) {
      fail("Unhandled exception: %s", e.what());
    } catch (...) {
      fail("Unhandled non-standard exception");
    }
  }

 private:
  enum class ScriptActionType : uint8_t {
    Press,
    Release,
    HomeTap,
    HomeLongPress,
    OpenSmokeBook,
    DisableReaderTouch,
    EnableReaderTouch,
    TouchDown,
    TouchMove,
    TouchRelease,
    AssertActivity,
    Render
  };

  struct ScriptAction {
    ScriptActionType type;
    MappedInputManager::Button button;
    const char* label;
    int settleFrames;
    int x;
    int y;
  };

  SmokeStep step = SmokeStep::Start;
  int settleFrames = 0;
  const char* activeStepName = nullptr;
  std::vector<ScriptAction> inputScript;
  size_t scriptIndex = 0;

  static bool enabled() { return std::getenv("CROSSINK_SIMULATOR_SMOKE_TEST") != nullptr; }

  static int pageTurnCount() {
    const char* raw = std::getenv("CROSSINK_SIMULATOR_SMOKE_PAGE_TURNS");
    if (raw == nullptr || raw[0] == '\0') {
      return 2;
    }
    return std::max(0, std::atoi(raw));
  }

  static void applyRequestedTheme() {
    const char* raw = std::getenv("CROSSINK_SIMULATOR_SMOKE_THEME");
    if (raw == nullptr || raw[0] == '\0') {
      return;
    }

    const int theme = std::atoi(raw);
    if (theme < 0 || theme >= CrossPointSettings::UI_THEME_COUNT) {
      fail("Invalid smoke test theme index: %d", theme);
    }

    SETTINGS.uiTheme = static_cast<uint8_t>(theme);
    UITheme::getInstance().reload();
    LOG_INF("SMOKE", "Using theme index %d", theme);
  }

  [[noreturn]] static void fail(const char* message) {
    LOG_ERR("SMOKE", "%s", message);
    std::_Exit(2);
  }

  template <typename... Args>
  [[noreturn]] static void fail(const char* format, Args... args) {
    logPrintf("ERR", "SMOKE", format, args...);
    logPrintf("ERR", "SMOKE", "\n");
    std::_Exit(2);
  }

  // Index into the flattened (category, app) space the walker is currently at.
  size_t smokeCategoryIndex = 0;
  size_t smokeAppIndex = 0;
  bool qrInitialCancelCovered = false;

  // Push the next registered app, or return false when every one has been
  // rendered. Each call advances by one app so the queueStep()/settle machinery
  // gets a chance to actually render it.
  bool advanceThroughRegisteredApps() {
    size_t categoryCount = 0;
    const AppCategoryInfo* categories = appCategories(&categoryCount);

    while (smokeCategoryIndex < categoryCount) {
      size_t appCount = 0;
      const AppEntry* apps = appsInCategory(categories[smokeCategoryIndex].category, &appCount);
      if (apps == nullptr || smokeAppIndex >= appCount) {
        ++smokeCategoryIndex;
        smokeAppIndex = 0;
        continue;
      }
      const AppEntry& entry = apps[smokeAppIndex];
      ++smokeAppIndex;
      auto activity = entry.create(renderer, mappedInputManager);
      if (!activity) {
        fail("App factory returned nullptr for %s", I18N.get(entry.title));
      }
      activityManager.replaceActivity(std::move(activity));
      if (entry.title == StrId::STR_APP_QR_GENERATOR) {
        const bool initialCancel = !qrInitialCancelCovered;
        if (initialCancel) {
          qrInitialCancelCovered = true;
          --smokeAppIndex;  // Reopen this app for the full input script next.
        }
        buildQrInputScript(initialCancel);
        queueStep(I18N.get(entry.title), SmokeStep::QrInput);
      } else if (entry.title == StrId::STR_APP_CIPHER) {
        buildCipherInputScript();
        queueStep(I18N.get(entry.title), SmokeStep::CipherInput);
      } else if (entry.title == StrId::STR_APP_CLOCK || entry.title == StrId::STR_APP_OTP_GENERATOR ||
                 entry.title == StrId::STR_APP_GAME_OF_LIFE) {
        buildBasicAppInputScript(entry.title);
        queueStep(I18N.get(entry.title), SmokeStep::BasicAppInput);
      } else {
        queueStep(I18N.get(entry.title), SmokeStep::AppScreens);
      }
      return true;
    }
    return false;
  }

  static void renderCurrentStep(const char* name) {
    LOG_INF("SMOKE", "Rendering %s", name);
    if (activityManager.requestUpdateAndWait() != RequestUpdateResult::Rendered) {
      fail("Render was rejected for %s", name);
    }
  }

  void queueStep(const char* name, SmokeStep nextStep, int framesToSettle = 3) {
    activeStepName = name;
    settleFrames = framesToSettle;
    step = nextStep;
  }

  void tickImpl() {
    mappedInputManager.simulatorClearInputFrame();

    if (settleFrames > 0) {
      --settleFrames;
      if (settleFrames == 0 && activeStepName != nullptr) {
        renderCurrentStep(activeStepName);
        activeStepName = nullptr;
      }
      return;
    }

    switch (step) {
      case SmokeStep::Start:
        LOG_INF("SMOKE", "Starting simulator smoke test");
        if (!CrossPointSettings::verifySleepTimeoutMigrationContract()) {
          fail("Sleep timeout migration contract failed");
        }
        if (!CrossPointSettings::verifySleepScreenMigrationContract()) {
          fail("Sleep screen migration contract failed");
        }
        if (!SimulatorHomeKeyInput::verifyTimingContract()) {
          fail("Simulator Home key timing contract failed");
        }
        if (!verifySimulatorCssCacheContract()) {
          fail("Compound CSS cache contract failed");
        }
        if (!verifySimulatorGameOfLifeRules(renderer, mappedInputManager)) {
          fail("Game of Life rules contract failed");
        }
        applyRequestedTheme();
        activityManager.goHome();
        queueStep("Home", SmokeStep::Home);
        break;

      case SmokeStep::Home:
        activityManager.goToFileBrowser("/books");
        queueStep("File Browser", SmokeStep::FileBrowser);
        break;

      case SmokeStep::FileBrowser:
        activityManager.goToRecentBooks();
        queueStep("Recent Books", SmokeStep::RecentBooks);
        break;

      case SmokeStep::RecentBooks:
        activityManager.goToSettings();
        queueStep("Settings", SmokeStep::Settings);
        break;

      case SmokeStep::Settings:
        activityManager.replaceActivity(std::make_unique<AppLauncherActivity>(renderer, mappedInputManager));
        queueStep("Tools launcher", SmokeStep::AppLauncher);
        break;

      case SmokeStep::AppLauncher:
        // The Tools tile, which is the one with apps in it today.
        activityManager.replaceActivity(
            std::make_unique<AppLauncherActivity>(renderer, mappedInputManager, AppCategory::Tools));
        queueStep("Tools category", SmokeStep::AppCategoryList);
        break;

      case SmokeStep::BasicAppInput:
      case SmokeStep::CipherInput:
      case SmokeStep::QrInput:
        runReaderInputScript(SmokeStep::AppScreens);
        break;

      case SmokeStep::AppScreens:
      case SmokeStep::AppCategoryList: {
        // Walk every app in the registry, entering and rendering each one.
        // Driven off the registry rather than a hardcoded list, so an app added
        // later is covered here the moment it is registered -- this is the
        // tripwire that catches an onEnter()/render() that crashes on entry.
        if (!advanceThroughRegisteredApps()) {
          activityManager.replaceActivity(std::make_unique<ReaderOptionsActivity>(renderer, mappedInputManager));
          queueStep("Reader Options", SmokeStep::ReaderOptions);
        }
        break;
      }

      case SmokeStep::ReaderOptions:
        activityManager.replaceActivity(
            std::make_unique<EpubReaderMenuActivity>(renderer, mappedInputManager, "Smoke Test", 1, 1, 0,
                                                     SETTINGS.orientation, false, false, false, false, false, false));
        queueStep("Reader Menu", SmokeStep::ReaderMenu);
        break;

      case SmokeStep::ReaderMenu:
        activityManager.goToSleep();
        queueStep("Sleep", SmokeStep::Sleep);
        break;

      case SmokeStep::Sleep: {
        const char* bookPath = std::getenv("CROSSINK_SIMULATOR_SMOKE_BOOK");
        if (bookPath == nullptr || bookPath[0] == '\0') {
          LOG_INF("SMOKE", "Skipping Reader step; CROSSINK_SIMULATOR_SMOKE_BOOK is not set");
          step = SmokeStep::Reader;
          break;
        }
        if (!Storage.exists(bookPath)) {
          fail("Smoke test book is missing: %s", bookPath);
        }
        activityManager.goToReader(bookPath, true);
        queueStep("Reader", SmokeStep::Reader, 8);
        break;
      }

      case SmokeStep::Reader:
        buildReaderInputScript();
        step = SmokeStep::ReaderInput;
        break;

      case SmokeStep::ReaderInput:
        runReaderInputScript();
        break;

      case SmokeStep::Done:
        LOG_INF("SMOKE", "Simulator smoke test passed");
        std::_Exit(0);
    }
  }

  static ScriptAction press(MappedInputManager::Button button) {
    return {ScriptActionType::Press, button, nullptr, 0, 0, 0};
  }

  static ScriptAction release(MappedInputManager::Button button) {
    return {ScriptActionType::Release, button, nullptr, 0, 0, 0};
  }

  static ScriptAction homeTap() {
    return {ScriptActionType::HomeTap, MappedInputManager::Button::Back, nullptr, 0, 0, 0};
  }

  static ScriptAction homeLongPress() {
    return {ScriptActionType::HomeLongPress, MappedInputManager::Button::Back, nullptr, 0, 0, 0};
  }

  static ScriptAction openSmokeBook() {
    return {ScriptActionType::OpenSmokeBook, MappedInputManager::Button::Back, nullptr, 0, 0, 0};
  }

  static ScriptAction disableReaderTouch() {
    return {ScriptActionType::DisableReaderTouch, MappedInputManager::Button::Back, nullptr, 0, 0, 0};
  }

  static ScriptAction enableReaderTouch() {
    return {ScriptActionType::EnableReaderTouch, MappedInputManager::Button::Back, nullptr, 0, 0, 0};
  }

  static ScriptAction render(const char* label, int framesToSettle = 3) {
    return {ScriptActionType::Render, MappedInputManager::Button::Back, label, framesToSettle, 0, 0};
  }

#if CROSSINK_APP_CAP_TOUCH
  static ScriptAction touchDown(const int x, const int y) {
    return {ScriptActionType::TouchDown, MappedInputManager::Button::Back, nullptr, 0, x, y};
  }
  static ScriptAction touchMove(const int x, const int y) {
    return {ScriptActionType::TouchMove, MappedInputManager::Button::Back, nullptr, 0, x, y};
  }
  static ScriptAction touchRelease(const int x, const int y) {
    return {ScriptActionType::TouchRelease, MappedInputManager::Button::Back, nullptr, 0, x, y};
  }
  static ScriptAction assertActivity(const char* name) {
    return {ScriptActionType::AssertActivity, MappedInputManager::Button::Back, name, 0, 0, 0};
  }
#endif

  void addTap(MappedInputManager::Button button) {
    inputScript.push_back(press(button));
    inputScript.push_back(release(button));
  }

  void buildQrInputScript(bool initialCancel) {
    inputScript.clear();
    inputScript.reserve(32);
    scriptIndex = 0;
    const auto expect = [](const char* name) -> ScriptAction {
      return {ScriptActionType::AssertActivity, MappedInputManager::Button::Back, name, 0, 0, 0};
    };
    inputScript.push_back(expect("KeyboardEntry"));
    if (initialCancel) {
      addTap(MappedInputManager::Button::Back);
      inputScript.push_back(render("Home after initial QR cancellation", 4));
      inputScript.push_back(expect("Home"));
      return;
    }
    // The builtin keyboard starts at digit 1. Up wraps to the action row;
    // Left wraps from its first key to OK. Drive the actual button API.
    addTap(MappedInputManager::Button::Confirm);
    addTap(MappedInputManager::Button::Up);
    addTap(MappedInputManager::Button::Left);
    addTap(MappedInputManager::Button::Confirm);
    inputScript.push_back(render("QR output after keyboard entry", 4));
    inputScript.push_back(expect("QrGenerator"));
    addTap(MappedInputManager::Button::Confirm);
    inputScript.push_back(render("QR edit keyboard", 4));
    inputScript.push_back(expect("KeyboardEntry"));
    addTap(MappedInputManager::Button::Back);
    inputScript.push_back(render("QR output retained after cancel", 4));
    inputScript.push_back(expect("QrGenerator"));
    addTap(MappedInputManager::Button::Back);
    inputScript.push_back(render("Home after QR exit", 4));
    inputScript.push_back(expect("Home"));
  }

  void buildCipherInputScript() {
    inputScript.clear();
    inputScript.reserve(512);
    scriptIndex = 0;
    const auto expect = [this](const char* name) {
      inputScript.push_back(render(name, 4));
      inputScript.push_back({ScriptActionType::AssertActivity, MappedInputManager::Button::Back, name, 0, 0, 0});
    };
    const auto submit = [this]() {
      // From the initial digit-1 selection, wrap to the OK action.
      addTap(MappedInputManager::Button::Up);
      addTap(MappedInputManager::Button::Left);
      addTap(MappedInputManager::Button::Confirm);
    };
    // Registry order: ROT13, Caesar, Vigenere, XOR encode/decode, Atbash,
    // Base64 encode/decode. Some digit-1 inputs intentionally produce errors;
    // this script checks navigation, while host tests check transform values.
    for (int algorithm = 0; algorithm < 8; ++algorithm) {
      const bool needsKey = algorithm >= 1 && algorithm <= 4;
      expect("Cipher");
      addTap(MappedInputManager::Button::Confirm);
      expect("KeyboardEntry");
      addTap(MappedInputManager::Button::Back);
      expect("Cipher");  // Cancel input returns to algorithm selection.
      addTap(MappedInputManager::Button::Confirm);
      expect("KeyboardEntry");
      addTap(MappedInputManager::Button::Confirm);  // Append digit 1.
      submit();
      if (needsKey) {
        expect("KeyboardEntry");
        addTap(MappedInputManager::Button::Back);
        expect("Cipher");  // Cancel key returns to selection.
        addTap(MappedInputManager::Button::Confirm);
        expect("KeyboardEntry");
        submit();  // Reuse the input retained after cancelling the key.
        expect("KeyboardEntry");
        addTap(MappedInputManager::Button::Confirm);
        submit();
      }
      expect("Cipher");                             // Result (including invalid-input results).
      addTap(MappedInputManager::Button::Confirm);  // Retry from result.
      expect("KeyboardEntry");
      addTap(MappedInputManager::Button::Back);
      expect("Cipher");
      addTap(MappedInputManager::Button::Down);
    }
    addTap(MappedInputManager::Button::Back);
    expect("Home");
  }

  void buildBasicAppInputScript(StrId title) {
    inputScript.clear();
    inputScript.reserve(24);
    scriptIndex = 0;
    const char* name = title == StrId::STR_APP_CLOCK           ? "Clock"
                       : title == StrId::STR_APP_OTP_GENERATOR ? "OtpGenerator"
                                                               : "GameOfLife";
    const auto expect = [this](const char* activity) {
      inputScript.push_back(render(activity, 4));
      inputScript.push_back({ScriptActionType::AssertActivity, MappedInputManager::Button::Back, activity, 0, 0, 0});
    };
    expect(name);
    if (title != StrId::STR_APP_CLOCK) {
      addTap(MappedInputManager::Button::Confirm);
      expect(name);
      if (title == StrId::STR_APP_OTP_GENERATOR) {
        addTap(MappedInputManager::Button::Down);
        expect(name);
      }
      addTap(MappedInputManager::Button::Up);
      expect(name);
      addTap(MappedInputManager::Button::Confirm);
      expect(name);
    }
    addTap(MappedInputManager::Button::Back);
    expect("Home");
  }

  void buildReaderInputScript() {
    inputScript.clear();
    scriptIndex = 0;

    const int turns = pageTurnCount();
#if CROSSINK_APP_CAP_TOUCH
    if (mappedInputManager.hasTouch()) {
      const int width = renderer.getScreenWidth();
      const int height = renderer.getScreenHeight();
      if (width <= 0 || height <= 0) fail("Touch smoke test has invalid screen dimensions");
      LOG_INF("SMOKE", "Running touch reader input script with %d page turn(s)", turns);
      for (int i = 0; i < turns; ++i) {
        inputScript.push_back(touchDown(width * 5 / 6, height / 2));
        inputScript.push_back(touchRelease(width * 5 / 6, height / 2));
        inputScript.push_back(render("Reader after touch page forward", 4));
      }
      if (mappedInputManager.hasHomeKey()) {
        // X4 Pro reserves the top-edge swipe for its frontlight overlay and
        // moves the reader menu to the bottom edge.
        inputScript.push_back(touchDown(width / 2, 8));
        inputScript.push_back(touchMove(width / 2, height / 4));
        inputScript.push_back(touchRelease(width / 2, height / 4));
        inputScript.push_back(render("Frontlight Panel opened from touch gesture", 4));
        inputScript.push_back(assertActivity("FrontlightPanel"));
        inputScript.push_back(touchDown(width / 2, height * 3 / 4));
        inputScript.push_back(touchRelease(width / 2, height * 3 / 4));
        inputScript.push_back(render("Reader restored after dismissing Frontlight Panel", 4));
        inputScript.push_back(assertActivity("EpubReader"));
        inputScript.push_back(homeLongPress());
        inputScript.push_back(render("Reader Menu opened from simulated Home key hold", 4));
        inputScript.push_back(assertActivity("EpubReaderMenu"));
        inputScript.push_back(touchDown(width / 2, 8));
        inputScript.push_back(touchMove(width / 2, height / 4));
        inputScript.push_back(touchRelease(width / 2, height / 4));
        inputScript.push_back(render("Reader restored after top-edge swipe dismisses Reader Menu", 4));
        inputScript.push_back(assertActivity("EpubReader"));
        inputScript.push_back(disableReaderTouch());
        inputScript.push_back(homeTap());
        inputScript.push_back(render("Home opened from simulated Home key tap", 4));
        inputScript.push_back(assertActivity("Home"));
        inputScript.push_back(enableReaderTouch());
        inputScript.push_back(openSmokeBook());
        inputScript.push_back(render("Reader reopened after simulated Home key tap", 8));
        inputScript.push_back(assertActivity("EpubReader"));
        inputScript.push_back(touchDown(width / 2, height - 8));
        inputScript.push_back(touchMove(width / 2, height * 3 / 4));
        inputScript.push_back(touchRelease(width / 2, height * 3 / 4));
      } else {
        inputScript.push_back(touchDown(width / 2, 8));
        inputScript.push_back(touchMove(width / 2, height / 4));
        inputScript.push_back(touchRelease(width / 2, height / 4));
      }
      inputScript.push_back(render("Reader Menu opened from touch gesture", 4));
      inputScript.push_back(assertActivity("EpubReaderMenu"));

      // The menu itself registers its tab and list hit areas through the active
      // theme. Exercise both before activating Reader Options from list row 1.
      const auto& metrics = UITheme::getInstance().getMetrics();
      const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, !mappedInputManager.hasTouch(), false);
      const int tabHeight = metrics.tabBarHeight * 2;
      const bool tabsAtBottom = mappedInputManager.hasHomeKey();
      const int tabTop = tabsAtBottom ? safe.y + safe.height - tabHeight
                                      : safe.y + metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight;
      const int listTop = safe.y + metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight +
                          (tabsAtBottom ? 0 : tabHeight) + metrics.verticalSpacing;
      const int rowHeight = uiThemeTokens(makeUiTarget(renderer)).rowHeight;
      if (rowHeight <= 0) fail("Touch smoke test has invalid list row height");

      inputScript.push_back(touchDown(safe.x + safe.width / 2, tabTop + tabHeight / 2));
      inputScript.push_back(touchRelease(safe.x + safe.width / 2, tabTop + tabHeight / 2));
      inputScript.push_back(render("Reader Menu tab touch navigation", 3));
      inputScript.push_back(assertActivity("EpubReaderMenu"));
      inputScript.push_back(touchDown(safe.x + safe.width / 6, tabTop + tabHeight / 2));
      inputScript.push_back(touchRelease(safe.x + safe.width / 6, tabTop + tabHeight / 2));
      inputScript.push_back(render("Reader Menu main tab restored", 3));
      inputScript.push_back(assertActivity("EpubReaderMenu"));

      const int readerOptionsY = listTop + rowHeight + rowHeight / 2;
      inputScript.push_back(touchDown(safe.x + safe.width / 2, readerOptionsY));
      inputScript.push_back(touchRelease(safe.x + safe.width / 2, readerOptionsY));
      inputScript.push_back(render("Reader Options opened by touch list activation", 4));
      inputScript.push_back(assertActivity("ReaderOptions"));

      const int optionsListTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
      const int optionsStartY = optionsListTop + rowHeight * 4 + rowHeight / 2;
      inputScript.push_back(touchDown(width / 2, optionsStartY));
      inputScript.push_back(touchMove(width / 2, optionsListTop + rowHeight / 2));
      inputScript.push_back(touchRelease(width / 2, optionsListTop + rowHeight / 2));
      inputScript.push_back(render("Reader Options touch swipe navigation", 3));
      inputScript.push_back(assertActivity("ReaderOptions"));
      return;
    }
#endif
    for (int i = 0; i < turns; i++) {
      addTap(MappedInputManager::Button::PageForward);
      inputScript.push_back(render("Reader after page forward", 4));
    }

    addTap(MappedInputManager::Button::Confirm);
    inputScript.push_back(render("Reader Menu opened from EPUB", 4));

    addTap(MappedInputManager::Button::Down);
    inputScript.push_back(render("Reader Menu Reader Options selection", 3));

    addTap(MappedInputManager::Button::Confirm);
    inputScript.push_back(render("Reader Options opened from Reader Menu", 4));

    addTap(MappedInputManager::Button::Down);
    inputScript.push_back(render("Reader Options after navigation", 3));

    addTap(MappedInputManager::Button::Confirm);
    inputScript.push_back(render("Reader Options after toggle", 3));

    addTap(MappedInputManager::Button::Back);
    inputScript.push_back(render("Reader Menu after closing Reader Options", 4));

    addTap(MappedInputManager::Button::Back);
    inputScript.push_back(render("Reader after closing Reader Menu", 4));

    LOG_INF("SMOKE", "Running reader input script with %d page turn(s)", turns);
  }

  void runReaderInputScript(SmokeStep nextStep = SmokeStep::Done) {
    if (scriptIndex >= inputScript.size()) {
      step = nextStep;
      return;
    }

    const auto& action = inputScript[scriptIndex++];
    switch (action.type) {
      case ScriptActionType::Press:
        mappedInputManager.simulatorInjectPress(action.button);
        break;
      case ScriptActionType::Release:
        mappedInputManager.simulatorInjectRelease(action.button);
        break;
      case ScriptActionType::HomeTap:
        simulatorHomeKeyInput.injectTap();
        break;
      case ScriptActionType::HomeLongPress:
        simulatorHomeKeyInput.injectLongPress();
        break;
      case ScriptActionType::OpenSmokeBook: {
        const char* bookPath = std::getenv("CROSSINK_SIMULATOR_SMOKE_BOOK");
        if (bookPath == nullptr || bookPath[0] == '\0') fail("Smoke test book path is missing");
        activityManager.goToReader(bookPath, true);
        break;
      }
      case ScriptActionType::DisableReaderTouch:
        SETTINGS.disableReaderTouchscreen = true;
        break;
      case ScriptActionType::EnableReaderTouch:
        SETTINGS.disableReaderTouchscreen = false;
        break;
      case ScriptActionType::TouchDown:
#if CROSSINK_APP_CAP_TOUCH
        mappedInputManager.simulatorInjectTouchDown(action.x, action.y);
#endif
        break;
      case ScriptActionType::TouchMove:
#if CROSSINK_APP_CAP_TOUCH
        mappedInputManager.simulatorInjectTouchMove(action.x, action.y);
#endif
        break;
      case ScriptActionType::TouchRelease:
#if CROSSINK_APP_CAP_TOUCH
        mappedInputManager.simulatorInjectTouchRelease(action.x, action.y);
#endif
        break;
      case ScriptActionType::AssertActivity:
        if (!activityManager.isCurrentActivityNamed(action.label)) fail("Expected current activity: %s", action.label);
        break;
      case ScriptActionType::Render:
        queueStep(action.label, step, action.settleFrames);
        break;
    }
  }
};

SimulatorSmokeTest smokeTest;

}  // namespace

void runSimulatorSmokeTestTick() { smokeTest.tick(); }

#endif
