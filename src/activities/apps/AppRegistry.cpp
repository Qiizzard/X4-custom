#include "AppRegistry.h"

#include <Memory.h>

#include "activities/Activity.h"
#include "activities/home/FileBrowserActivity.h"
#include "activities/network/WifiSelectionActivity.h"
#include "barcode/BarcodeActivity.h"
#include "calculator/CalculatorActivity.h"
#include "cipher/CipherActivity.h"
#include "clock/ClockActivity.h"
#include "countdown/CountdownActivity.h"
#include "dice_roller/DiceRollerActivity.h"
#include "dns_lookup/DnsLookupActivity.h"
#include "etch_a_sketch/EtchASketchActivity.h"
#include "event_logger/EventLoggerActivity.h"
#include "flashcards/FlashcardActivity.h"
#include "game_of_life/GameOfLifeActivity.h"
#include "habit_tracker/HabitTrackerActivity.h"
#include "key_copier/KeyCopierActivity.h"
#include "maze/MazeActivity.h"
#include "mdns_browser/MdnsBrowserActivity.h"
#include "minesweeper/MinesweeperActivity.h"
#include "morse_code/MorseCodeActivity.h"
#include "otp_generator/OtpGeneratorActivity.h"
#include "qr_generator/QrGeneratorActivity.h"
#include "secure_vault/PasswordManagerActivity.h"
#include "snake/SnakeActivity.h"
#include "stego_notes/StegoNotesActivity.h"
#include "sudoku/SudokuActivity.h"
#include "tetris/TetrisActivity.h"
#include "unit_converter/UnitConverterActivity.h"
#include "wifi_scanner/WifiScannerActivity.h"

namespace {

// One factory per app. makeUniqueNoThrow returns nullptr instead of aborting
// when the heap cannot satisfy the allocation (rule 6); the launcher handles
// the nullptr.
template <typename T>
std::unique_ptr<Activity> makeApp(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<T>(renderer, mappedInput);
}

std::unique_ptr<Activity> makeAuthenticator(GfxRenderer& renderer, MappedInputManager& input) {
  return makeUniqueNoThrow<PasswordManagerActivity>(renderer, input, PasswordManagerActivity::Mode::Authenticator);
}
std::unique_ptr<Activity> makeTotpQr(GfxRenderer& renderer, MappedInputManager& input) {
  return makeUniqueNoThrow<PasswordManagerActivity>(renderer, input, PasswordManagerActivity::Mode::TotpQr);
}

// THE TABLE. constexpr + static => flash, not DRAM.
//
// Order within a category is the order the launcher shows. Keep new rows
// grouped by category; appsInCategory() scans linearly, which is free at this
// size and keeps the table readable.
constexpr AppCategoryInfo kCategories[] = {
    {AppCategory::Tools, StrId::STR_APP_CAT_TOOLS}, {AppCategory::Games, StrId::STR_APP_CAT_GAMES},
    {AppCategory::Recon, StrId::STR_APP_CAT_RECON}, {AppCategory::Defense, StrId::STR_APP_CAT_DEFENSE},
    {AppCategory::Comms, StrId::STR_APP_CAT_COMMS},
};

static_assert(sizeof(kCategories) / sizeof(kCategories[0]) == kAppCategoryCount,
              "kAppCategoryCount must match the category table");

constexpr AppEntry kApps[] = {
    // ---- Tools ----
    {AppCategory::Tools, StrId::STR_APP_MDNS_BROWSER, &makeApp<MdnsBrowserActivity>},
    {AppCategory::Tools, StrId::STR_APP_DNS_LOOKUP, &makeApp<DnsLookupActivity>},
    {AppCategory::Tools, StrId::STR_WIFI_NETWORKS, &makeApp<WifiSelectionActivity>},
    {AppCategory::Tools, StrId::STR_APP_CALCULATOR, &makeApp<CalculatorActivity>},
    {AppCategory::Tools, StrId::STR_APP_UNIT_CONVERTER, &makeApp<UnitConverterActivity>},
    {AppCategory::Tools, StrId::STR_APP_MORSE_CODE, &makeApp<MorseCodeActivity>},
    {AppCategory::Tools, StrId::STR_APP_COUNTDOWN, &makeApp<CountdownActivity>},
    {AppCategory::Tools, StrId::STR_APP_CLOCK, &makeApp<ClockActivity>},
    {AppCategory::Tools, StrId::STR_APP_QR_GENERATOR, &makeApp<QrGeneratorActivity>},
    {AppCategory::Tools, StrId::STR_APP_CIPHER, &makeApp<CipherActivity>},
    {AppCategory::Tools, StrId::STR_APP_OTP_GENERATOR, &makeApp<OtpGeneratorActivity>},

    {AppCategory::Tools, StrId::STR_APP_ETCH, &makeApp<EtchASketchActivity>},
    {AppCategory::Tools, StrId::STR_BROWSE_FILES, &makeApp<FileBrowserActivity>},

    {AppCategory::Tools, StrId::STR_APP_BARCODE, &makeApp<BarcodeActivity>},
    {AppCategory::Tools, StrId::STR_APP_KEY_CHARTS, &makeApp<KeyCopierActivity>},

    {AppCategory::Tools, StrId::STR_APP_EVENT_LOGGER, &makeApp<EventLoggerActivity>},

    {AppCategory::Tools, StrId::STR_APP_FLASHCARDS, &makeApp<FlashcardActivity>},

    {AppCategory::Tools, StrId::STR_APP_HABITS, &makeApp<HabitTrackerActivity>},

    {AppCategory::Tools, StrId::STR_VAULT_APP, &makeApp<PasswordManagerActivity>},

    {AppCategory::Tools, StrId::STR_TOTP_APP, &makeAuthenticator},
    {AppCategory::Tools, StrId::STR_TOTP_QR_APP, &makeTotpQr},

    {AppCategory::Tools, StrId::STR_STEGO_APP, &makeApp<StegoNotesActivity>},

    // ---- Games ----
    {AppCategory::Games, StrId::STR_APP_DICE_ROLLER, &makeApp<DiceRollerActivity>},
    {AppCategory::Games, StrId::STR_APP_GAME_OF_LIFE, &makeApp<GameOfLifeActivity>},

    {AppCategory::Games, StrId::STR_APP_SNAKE, &makeApp<SnakeActivity>},
    {AppCategory::Games, StrId::STR_APP_MINESWEEPER, &makeApp<MinesweeperActivity>},
    {AppCategory::Games, StrId::STR_APP_TETRIS, &makeApp<TetrisActivity>},

    {AppCategory::Games, StrId::STR_APP_SUDOKU, &makeApp<SudokuActivity>},

    {AppCategory::Games, StrId::STR_APP_MAZE, &makeApp<MazeActivity>},

    // ---- Recon ----
    {AppCategory::Recon, StrId::STR_APP_WIFI_SCANNER, &makeApp<WifiScannerActivity>},

};

constexpr size_t kAppCount = sizeof(kApps) / sizeof(kApps[0]);

// appsInCategory() returns a pointer to the first row of a category plus a
// count, which is only meaningful if each category is one contiguous run. That
// is an easy invariant to break by adding a row in the wrong place, and the
// symptom would be a launcher listing another tile's apps -- so it is checked
// at compile time rather than trusted.
constexpr bool categoriesAreContiguous() {
  for (size_t i = 0; i < kAppCount; ++i) {
    for (size_t j = i + 1; j < kAppCount; ++j) {
      if (kApps[i].category != kApps[j].category) continue;
      // Same category at i and j: every row between them must match too.
      for (size_t k = i + 1; k < j; ++k) {
        if (kApps[k].category != kApps[i].category) return false;
      }
    }
  }
  return true;
}

static_assert(categoriesAreContiguous(),
              "AppRegistry: rows must be grouped by category. Move the new row "
              "next to the others in its tile -- appsInCategory() returns a "
              "first-row pointer plus a count and cannot express a gap.");

// A row with no factory would look like an app and do nothing when selected.
constexpr bool everyEntryHasAFactory() {
  for (size_t i = 0; i < kAppCount; ++i) {
    if (kApps[i].create == nullptr) return false;
  }
  return true;
}

static_assert(everyEntryHasAFactory(), "AppRegistry: every entry needs a factory");

}  // namespace

const AppCategoryInfo* appCategories(size_t* outCount) {
  if (outCount != nullptr) *outCount = kAppCategoryCount;
  return kCategories;
}

const AppEntry* appsInCategory(const AppCategory category, size_t* outCount) {
  // The table is grouped by category, so a category is one contiguous run.
  size_t first = 0;
  bool found = false;
  size_t count = 0;
  for (size_t i = 0; i < kAppCount; ++i) {
    if (kApps[i].category != category) continue;
    if (!found) {
      first = i;
      found = true;
    }
    ++count;
  }
  if (outCount != nullptr) *outCount = count;
  return found ? &kApps[first] : nullptr;
}

size_t appCountInCategory(const AppCategory category) {
  size_t count = 0;
  appsInCategory(category, &count);
  return count;
}
