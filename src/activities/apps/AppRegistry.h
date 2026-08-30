#pragma once
// AppRegistry -- the single list of every app the launcher can open.
//
// One table, in flash, that maps a tile category to the apps in it. Adding an
// app is one row here plus its files; nothing else in the launcher changes.
//
// Why a table of function pointers rather than a switch or a vector of
// std::function: the table is `static constexpr` so it lives in DROM and costs
// no DRAM (RULESET rule 4), and a plain function pointer avoids the capture
// allocation std::function would make (AGENTS.md: no std::function in library
// code). The launcher never allocates an activity it does not open -- the
// factory is only called on select.

#include <cstddef>
#include <cstdint>
#include <memory>

#include "I18nKeys.h"

class Activity;
class GfxRenderer;
class MappedInputManager;

// The launcher tiles, in the order the brief lists them. Reader and Settings
// are deliberately absent: reading is the home screen, not a tile, and
// CrossInk's Settings screen is already reachable from Home.
enum class AppCategory : uint8_t {
  Tools,
  Games,
  Recon,
  Defense,
  Comms,
};

inline constexpr size_t kAppCategoryCount = 5;

struct AppCategoryInfo {
  AppCategory category;
  StrId title;
};

struct AppEntry {
  AppCategory category;
  StrId title;
  // Called only when the user selects the row. Returns nullptr if the activity
  // could not be allocated -- the launcher checks and reports rather than
  // dereferencing (rule 6: allocation can fail, and `new` aborts on ESP32).
  std::unique_ptr<Activity> (*create)(GfxRenderer& renderer, MappedInputManager& mappedInput);
};

// All five categories, in launcher order.
const AppCategoryInfo* appCategories(size_t* outCount);

// The apps in one category. Returns a pointer into the static table and writes
// the count; never allocates. An empty category returns count 0, which the
// launcher renders as "no apps yet" rather than an empty screen.
const AppEntry* appsInCategory(AppCategory category, size_t* outCount);

// How many apps a category holds, for the launcher's category rows.
size_t appCountInCategory(AppCategory category);
