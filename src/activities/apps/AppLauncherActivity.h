#pragma once
// AppLauncherActivity -- the Tools launcher.
//
// One screen, two levels: with no category it lists the tiles (Tools, Games,
// Recon, Defense, Comms); given a category it lists that tile's apps. Selecting
// a category pushes another instance of this same activity scoped to it, so
// Back unwinds naturally and there is only one list implementation to keep
// consistent.
//
// RULESET rule 19: this is built from the same FreeInkUI list, header and
// button-hint components every other CrossInk screen uses, so the launcher and
// the apps inside it share the reader's typography, spacing and button grammar.
// No app gets its own visual language, and neither does the launcher.

#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>

#include <atomic>
#include <optional>

#include "AppRegistry.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class AppLauncherActivity final : public Activity {
  // 32 interaction slots is well clear of the largest tile; 4 action ids is one
  // more than this screen registers.
  using UiApp = freeink::ui::FreeInkApp<32, 4>;

 public:
  // No category => the tile list. A category => that tile's apps.
  AppLauncherActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                      std::optional<AppCategory> category = std::nullopt);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void handleSelection();
  void openCategory(AppCategory category);
  void openApp(const AppEntry& entry);
  int itemCount() const;
  const char* title() const;

  std::optional<AppCategory> category;
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  freeink::ui::GfxRendererTarget uiTarget;
  UiApp app;
  std::atomic<bool> uiReady{false};
  int visibleRows = 1;
  int topIndex = 0;
  bool initialViewportPending = true;

  static void launcherScreen(UiApp::ScreenType& screen, void* user);
  static void onRowEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildLauncherScreen(UiApp::ScreenType& screen);
};
