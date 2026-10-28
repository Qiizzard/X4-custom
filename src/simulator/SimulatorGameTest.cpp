#ifdef SIMULATOR
#include "SimulatorGameTest.h"

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdlib>
#include <cstring>

#include "activities/apps/casino/CasinoActivity.h"
#include "activities/apps/chess/ChessActivity.h"
#include "activities/apps/event_logger/EventLoggerActivity.h"
#include "activities/apps/flashcards/FlashcardActivity.h"
#include "activities/apps/habit_tracker/HabitTrackerActivity.h"
#include "activities/apps/maze/MazeActivity.h"
#include "activities/apps/minesweeper/MinesweeperActivity.h"
#include "activities/apps/snake/SnakeActivity.h"
#include "activities/apps/sudoku/SudokuActivity.h"
#include "activities/apps/tetris/TetrisActivity.h"
#include "activities/apps/transit_alert/TransitAlertActivity.h"
extern GfxRenderer renderer;
extern MappedInputManager mappedInputManager;

// Simulator-only access to production game logic. No copied solver or test
// branches in gameplay code; this does not validate physical input/rendering.
class SimulatorGameTest {
  static void require(bool ok, const char* reason) {
    if (!ok) {
      LOG_ERR("GAMETEST", "%s", reason);
      std::_Exit(2);
    }
  }
  static void solve(SudokuActivity& game) {
    game.startSolving();
    unsigned steps = 0;
    while (game.state == SudokuActivity::SOLVING && steps++ < 65536) game.solveStep();
    require(game.state != SudokuActivity::SOLVING, "Sudoku solver exceeded bounded test budget");
  }

  static void casinoSettlement() {
    CasinoActivity game(renderer, mappedInputManager);
    struct Fixture {
      uint8_t player[3], dealer[2], count, next, draws;
      unsigned returned, total;
    };
    static constexpr Fixture cases[] = {
        {{1, 13, 0}, {10, 9}, 2, 5, 0, 250, 21}, {{1, 10, 0}, {1, 13}, 2, 5, 0, 100, 21},
        {{7, 7, 7}, {1, 10}, 3, 5, 0, 0, 21},    {{10, 9, 0}, {10, 7}, 2, 5, 0, 200, 19},
        {{10, 9, 3}, {10, 6}, 3, 5, 0, 0, 22},   {{7, 7, 7}, {10, 6}, 3, 5, 1, 100, 21},
        {{10, 8, 0}, {1, 6}, 2, 5, 0, 200, 18},  {{10, 8, 0}, {10, 6}, 2, 10, 1, 200, 18},
        {{1, 10, 0}, {10, 6}, 2, 5, 0, 250, 21}, {{10, 7, 0}, {10, 9}, 2, 5, 0, 0, 17},
    };
    for (const auto& f : cases) {
      memcpy(game.player, f.player, sizeof(f.player));
      memcpy(game.dealer, f.dealer, sizeof(f.dealer));
      game.playerCount = f.count;
      game.dealerCount = 2;
      game.position = 0;
      game.deck[0] = f.next;
      game.betIndex = 3;
      game.credits = 900;  // Wager already deducted by play().
      game.settleBlackjack(true);
      require(game.credits == 900 + f.returned && game.won == (f.returned > 100) &&
                  game.pushed == (f.returned == 100) && game.outcome == f.total && game.position == f.draws &&
                  game.dealerCount == 2 + f.draws && game.state == CasinoActivity::State::Result,
              "Casino Blackjack settlement/draw policy");
    }
    game.player[0] = 1;
    game.player[1] = 10;
    game.playerCount = 2;
    game.dealer[0] = 10;
    game.dealer[1] = 9;
    game.dealerCount = 2;
    game.betIndex = 1;
    game.credits = 975;
    game.settleBlackjack(false);
    require(game.credits == 1037, "Casino odd natural payout integer rounding");
    game.credits = game.cap - 1;
    game.settleBlackjack(false);
    require(game.credits == game.cap, "Casino Blackjack credit cap");

    // Explicit winning pockets, independent of the production classifier.
    static constexpr const char* pockets[] = {
        "1,3,5,7,9,12,14,16,18,19,21,23,25,27,30,32,34,36,",
        "2,4,6,8,10,11,13,15,17,20,22,24,26,28,29,31,33,35,",
        "1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31,33,35,",
        "2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,",
        "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,",
        "19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,",
        "1,2,3,4,5,6,7,8,9,10,11,12,",
        "13,14,15,16,17,18,19,20,21,22,23,24,",
        "25,26,27,28,29,30,31,32,33,34,35,36,",
    };
    game.mode = 2;
    game.betIndex = 3;
    for (unsigned choice = 0; choice < 9; ++choice) {
      game.choice = choice;
      uint64_t seen = 0;
      for (unsigned seed = 1; seed <= 256; ++seed) {
        game.rng = seed;
        game.credits = 1000;
        game.play();
        require(game.outcome <= 36, "Casino roulette pocket bounds");
        seen |= uint64_t(1) << game.outcome;
        bool expected = false;
        for (const char* cursor = pockets[choice]; *cursor;) {
          char* end;
          const unsigned value = std::strtoul(cursor, &end, 10);
          if (value == game.outcome) expected = true;
          cursor = end + 1;
        }
        require(game.won == expected && game.credits == 900 + (expected ? (choice < 6 ? 200 : 300) : 0),
                "Casino roulette outside/dozen wager settlement");
      }
      require(seen == (uint64_t(1) << 37) - 1, "Casino roulette wager fixture misses pocket");
    }
    LOG_INF("GAMETEST", "GAME TEST RESULT: PASS casino_settlement");
  }

  static void casinoGameplay() {
    CasinoActivity game(renderer, mappedInputManager);
    for (uint32_t seed = 1; seed <= 64; ++seed) {
      game.rng = seed;
      game.position = 52;
      unsigned ranks[14]{};
      for (unsigned i = 0; i < 52; ++i) {
        const auto card = game.drawCard();
        require(card >= 1 && card <= 13, "Casino deck card bounds");
        ++ranks[card];
      }
      for (unsigned rank = 1; rank <= 13; ++rank) require(ranks[rank] == 4, "Casino deck rank multiplicity");
      require(game.drawCard() >= 1 && game.position == 1, "Casino exhausted deck reshuffle");
    }
    game.mode = 0;
    game.betIndex = 3;  // 100 credits.
    game.credits = 99;
    game.dirty = false;
    game.rng = 123;
    game.play();
    require(game.insufficient && game.credits == 99 && !game.dirty && game.rng == 123,
            "Casino unaffordable wager mutates funds/RNG");
    uint64_t rouletteOutcomes = 0;
    for (unsigned mode : {0u, 2u}) {
      game.mode = mode;
      game.choice = mode == 0 ? 1 : 9;
      game.number = 7;
      for (uint32_t seed = 1; seed <= 256; ++seed) {
        game.rng = seed;
        game.credits = 1000;
        game.play();
        const bool expectedWin = game.outcome == (mode == 0 ? 1u : 7u);
        const unsigned payout = mode == 0 ? 200 : 3600;
        require(game.won == expectedWin && game.credits == 900 + (expectedWin ? payout : 0) &&
                    game.state == CasinoActivity::State::Result,
                "Casino coin/straight roulette settlement");
        require(game.outcome < (mode == 0 ? 2u : 37u), "Casino outcome range");
        if (mode == 2) rouletteOutcomes |= uint64_t(1) << game.outcome;
      }
    }
    require(rouletteOutcomes == (uint64_t(1) << 37) - 1, "Casino roulette fixture misses outcomes");
    game.mode = 1;
    game.position = 0;
    game.deck[0] = 7;
    game.card = 7;
    game.pot = 100;
    game.streak = 0;
    game.state = CasinoActivity::State::HighLow;
    game.guess(true);
    require(game.won && game.pot == 150 && game.streak == 1, "Casino equal-rank HighLow win");
    game.deck[1] = 6;
    game.guess(true);
    require(!game.won && game.pot == 0 && game.state == CasinoActivity::State::Result, "Casino HighLow loss");
    game.credits = game.cap - 1;
    game.pot = 500;
    game.cashOut();
    require(game.credits == game.cap && game.pot == 0 && game.won, "Casino capped cashout");
    const unsigned payouts[] = {50, 15, 50, 40, 80};
    game.mode = 4;
    for (unsigned machine = 0; machine < 5; ++machine) {
      game.machine = machine;
      game.credits = 1000;
      game.freeSpins = 0;
      game.held = 7;
      memset(game.reels, 0, sizeof(game.reels));
      game.doubled = true;
      game.play();
      require(game.won && game.outcome == 200 * payouts[machine] && game.credits == 900 + game.outcome &&
                  !game.doubled && game.held == 0 && game.reelsReady,
              "Casino held-reel double payout");
      require(game.freeSpins == (machine == 1 ? 3 : 0), "Casino fruit free-spin award");
    }
    game.machine = 0;
    game.credits = 0;
    game.freeSpins = 1;
    game.held = 7;
    memset(game.reels, 0, sizeof(game.reels));
    game.play();
    require(!game.insufficient && game.freeSpins == 0 && game.credits == 5000, "Casino free spin debits credits");
    for (uint32_t seed = 1; seed <= 64; ++seed) {
      game.rng = seed;
      game.choice = 1;
      game.credits = 450;
      memset(game.collected, 0, sizeof(game.collected));
      game.pullLoot();
      bool rare = false;
      require(game.pullCount == 5 && !game.insufficient && game.credits <= 100, "Casino five-pull cost/refunds");
      for (unsigned i = 0; i < game.pullCount; ++i) {
        require(game.pulls[i] < 50 && game.hasItem(game.pulls[i]), "Casino loot collection bounds");
        rare |= game.pulls[i] >= 20;
      }
      require(rare, "Casino five-pull guaranteed rare missing");
    }
    game.credits = 449;
    game.dirty = false;
    game.pullLoot();
    require(game.insufficient && game.credits == 449 && !game.dirty, "Casino insufficient loot purchase");
    LOG_INF("GAMETEST", "GAME TEST RESULT: PASS casino_gameplay");
  }

  static void casinoPersistence() {
    const char* paths[] = {"fs_/crossink/casino-a.dat", "fs_/crossink/casino-b.dat"};
    auto write = [&](int slot, const uint8_t* bytes, size_t size) {
      FILE* file = fopen(paths[slot], "wb");
      require(file && fwrite(bytes, 1, size, file) == size, "Casino fixture write");
      require(fclose(file) == 0, "Casino fixture close");
    };
    auto seal = [](uint8_t* bytes) {
      uint32_t sum = 2166136261u;
      for (unsigned i = 0; i < 19; ++i) sum = (sum ^ bytes[i]) * 16777619u;
      for (unsigned i = 0; i < 4; ++i) bytes[19 + i] = uint8_t(sum >> (8 * i));
    };
    CasinoActivity game(renderer, mappedInputManager);
    game.loadProgress();
    require(!game.saveBlocked && game.saveSlot == -1 && game.credits == 1000, "Casino empty progress");
    game.credits = game.cap;
    memset(game.collected, 255, 6);
    game.collected[6] = 3;
    game.saveProgress();
    require(!game.dirty && !game.saveError && game.saveSlot == 0 && game.saveGeneration == 1, "Casino first save");
    game.credits = 0;
    game.dirty = true;
    game.saveProgress();
    require(!game.dirty && game.saveSlot == 1 && game.saveGeneration == 2, "Casino alternate save");
    {
      CasinoActivity loaded(renderer, mappedInputManager);
      loaded.loadProgress();
      require(!loaded.saveBlocked && loaded.saveSlot == 1 && loaded.credits == 0 && !loaded.dirty,
              "Casino newest generation load");
      for (unsigned i = 0; i < 50; ++i) require(loaded.hasItem(i), "Casino collection round trip");
    }
    uint8_t good[23];
    FILE* file = fopen(paths[1], "rb");
    require(file && fread(good, 1, sizeof(good), file) == sizeof(good), "Casino snapshot read");
    require(fclose(file) == 0, "Casino snapshot close");
    for (unsigned defect = 0; defect < 6; ++defect) {
      uint8_t bad[23];
      memcpy(bad, good, sizeof(bad));
      if (defect == 0) bad[0] = 'X';
      if (defect == 1) memset(bad + 4, 0, 4);
      if (defect == 2) memset(bad + 8, 255, 4);
      if (defect == 3) bad[18] = 4;
      seal(bad);
      if (defect == 4) bad[19] ^= 1;
      write(1, bad, defect == 5 ? 22 : 23);
      CasinoActivity recovered(renderer, mappedInputManager);
      recovered.loadProgress();
      require(recovered.saveError && !recovered.saveBlocked && recovered.saveSlot == 0 &&
                  recovered.saveGeneration == 1 && recovered.credits == game.cap,
              "Casino malformed newest slot fallback");
    }
    uint8_t bad[] = {1, 2, 3};
    write(0, bad, sizeof(bad));
    write(1, bad, sizeof(bad));
    {
      CasinoActivity blocked(renderer, mappedInputManager);
      blocked.loadProgress();
      blocked.saveProgress();
      require(blocked.saveBlocked && blocked.saveError && blocked.dirty && blocked.saveSlot == -1,
              "Casino corrupt saves must block writes");
      for (const char* path : paths) {
        file = fopen(path, "rb");
        uint8_t actual[4]{};
        require(file && fread(actual, 1, sizeof(actual), file) == 3 && memcmp(actual, bad, 3) == 0,
                "Casino blocked save changed files");
        require(fclose(file) == 0, "Casino preservation close");
      }
    }
    memset(good + 4, 255, 4);
    seal(good);
    write(0, good, sizeof(good));
    {
      CasinoActivity exhausted(renderer, mappedInputManager);
      exhausted.loadProgress();
      exhausted.dirty = true;
      exhausted.saveProgress();
      require(
          !exhausted.saveBlocked && exhausted.saveGeneration == UINT32_MAX && exhausted.saveError && exhausted.dirty,
          "Casino generation wrap prevention");
    }
    require(remove(paths[0]) == 0 && remove(paths[1]) == 0, "Casino fixture cleanup");
    LOG_INF("GAMETEST", "GAME TEST RESULT: PASS casino_save");
  }

  static void transit() {
    uint8_t record[71]{};
    memcpy(record, "TRN1", 4);
    memset(record + 4, 'A', 31);  // Maximum supported name, terminated at byte 35.
    record[36] = 5;
    for (unsigned i = 0; i < 5; ++i) {
      record[37 + i * 6] = 2;
      record[38 + i * 6] = i + 1;
    }
    auto seal = [](uint8_t* bytes) {
      uint32_t sum = 2166136261u;
      for (unsigned i = 0; i < 67; ++i) sum = (sum ^ bytes[i]) * 16777619u;
      for (unsigned i = 0; i < 4; ++i) bytes[67 + i] = static_cast<uint8_t>(sum >> (8 * i));
    };
    auto pathFor = [](unsigned slot, char* path, size_t size) {
      snprintf(path, size, "fs_/crossink/transit/stop-%02u.dat", slot);
    };
    auto write = [&](unsigned slot, const uint8_t* bytes, size_t size) {
      char path[64];
      pathFor(slot, path, sizeof(path));
      FILE* file = fopen(path, "wb");
      require(file && fwrite(bytes, 1, size, file) == size, "Transit fixture write");
      require(fclose(file) == 0, "Transit fixture close");
    };
    seal(record);
    TransitAlertActivity game(renderer, mappedInputManager);
    game.load();
    require(game.count == 0 && !game.storageError, "Transit empty storage");
    for (unsigned slot : {0u, 7u, 31u}) write(slot, record, sizeof(record));
    game.load();
    require(game.count == 3 && game.slots[0] == 0 && game.slots[1] == 7 && game.slots[2] == 31 &&
                strlen(game.stops[2].name) == 31 && game.stops[2].count == 5 &&
                memcmp(game.stops[2].bssids, record + 37, 30) == 0,
            "Transit sparse slot mapping and maximum fields");
    for (unsigned slot = 0; slot < 32; ++slot) write(slot, record, sizeof(record));
    write(32, record, sizeof(record));  // Outside the bounded slot namespace.
    game.load();
    require(game.count == 32 && game.slots[31] == 31 && !game.storageError, "Transit bounded 32-stop load");
    for (unsigned defect = 0; defect < 10; ++defect) {
      uint8_t bad[71];
      memcpy(bad, record, sizeof(bad));
      if (defect == 0) bad[0] = 'X';
      if (defect == 1) bad[4] = 0;
      if (defect == 2) bad[35] = 'x';
      if (defect == 3) bad[5] = 31;
      if (defect == 4) bad[36] = 0;
      if (defect == 5) bad[36] = 6;
      if (defect == 6) memset(bad + 37, 0, 6);
      if (defect == 7) bad[37] = 3;  // Multicast address.
      if (defect == 8) memcpy(bad + 43, bad + 37, 6);
      seal(bad);
      if (defect == 9) bad[67] ^= 1;
      write(7, bad, sizeof(bad));
      game.storageError = false;
      game.load();
      require(game.storageError && game.count == 31 && game.slots[7] == 8 && game.slots[30] == 31,
              "Transit malformed stop skipped without slot aliasing");
      char path[64];
      pathFor(7, path, sizeof(path));
      FILE* file = fopen(path, "rb");
      uint8_t unchanged[72]{};
      require(file && fread(unchanged, 1, sizeof(unchanged), file) == 71 && memcmp(unchanged, bad, 71) == 0,
              "Transit malformed source was modified");
      require(fclose(file) == 0, "Transit preserved fixture close");
    }
    write(7, record, 70);
    game.load();
    require(game.count == 31 && game.storageError, "Transit truncated stop accepted");
    write(7, record, sizeof(record));
    game.storageError = false;
    game.load();
    require(game.count == 32 && !game.storageError, "Transit repaired fixture reload");
    for (unsigned slot = 0; slot <= 32; ++slot) {
      char path[64];
      pathFor(slot, path, sizeof(path));
      require(remove(path) == 0, "Transit fixture cleanup");
    }
    LOG_INF("GAMETEST", "GAME TEST RESULT: PASS transit");
  }

  static void habits() {
    const char* paths[] = {"fs_/crossink/habits-a.dat", "fs_/crossink/habits-b.dat"};
    auto writeSlot = [&](int slot, const uint8_t* bytes, size_t size) {
      FILE* file = fopen(paths[slot], "wb");
      require(file && fwrite(bytes, 1, size, file) == size, "Habits fixture write");
      require(fclose(file) == 0, "Habits fixture close");
    };
    HabitTrackerActivity game(renderer, mappedInputManager);
    game.load();
    require(!game.blocked && game.count == 0 && game.activeSlot == -1, "Habits empty storage");
    game.count = 10;
    for (int i = 0; i < 10; ++i) {
      snprintf(game.habits[i].name, sizeof(game.habits[i].name), "Habit %d", i);
      game.habits[i].done = i % 2;
      game.habits[i].streak = i;
      game.habits[i].best = i + 2;
    }
    game.dirty = true;
    require(game.save() && game.activeSlot == 0 && game.generation == 1 && !game.dirty, "Habits first save");
    game.session = 2;
    game.dirty = true;
    require(game.save() && game.activeSlot == 1 && game.generation == 2, "Habits alternate slot save");
    {
      HabitTrackerActivity loaded(renderer, mappedInputManager);
      loaded.load();
      require(!loaded.blocked && loaded.activeSlot == 1 && loaded.session == 2 && loaded.count == 10 &&
                  strcmp(loaded.habits[9].name, "Habit 9") == 0 && loaded.habits[9].done &&
                  loaded.habits[9].streak == 9 && loaded.habits[9].best == 11,
              "Habits newest generation round trip");
    }
    // Simulator-only snapshots may exceed embedded stack limits; these never ship.
    uint8_t good[427];
    memcpy(good, game.bytes, sizeof(good));
    for (int defect = 0; defect < 8; ++defect) {
      memcpy(game.bytes, good, sizeof(good));
      if (defect == 0) game.bytes[0] = 'X';
      if (defect == 1) game.bytes[12] = 11;
      if (defect == 2) game.put32(8, 0);
      if (defect == 3) game.bytes[13] = 0;
      if (defect == 4) memset(game.bytes + 13, 'x', 32);
      if (defect == 5) game.bytes[45] = 2;
      if (defect == 6) game.put32(50, 0);  // best below streak in the first record
      if (defect == 6) game.put32(46, 1);
      game.put32(423, game.checksum(game.bytes, 423));
      if (defect == 7) game.bytes[423] ^= 1;
      writeSlot(1, game.bytes, sizeof(game.bytes));
      HabitTrackerActivity recovered(renderer, mappedInputManager);
      recovered.load();
      require(!recovered.blocked && recovered.activeSlot == 0 && recovered.generation == 1 && recovered.session == 1,
              "Habits corrupt newest slot fallback");
    }
    writeSlot(1, good, 100);
    {
      HabitTrackerActivity recovered(renderer, mappedInputManager);
      recovered.load();
      require(!recovered.blocked && recovered.activeSlot == 0, "Habits truncated newest fallback");
    }
    const uint8_t bad[] = {1, 2, 3};
    writeSlot(0, bad, sizeof(bad));
    writeSlot(1, bad, sizeof(bad));
    {
      HabitTrackerActivity blocked(renderer, mappedInputManager);
      blocked.load();
      blocked.dirty = true;
      require(blocked.blocked && blocked.error && !blocked.save(), "Habits both corrupt must block writes");
      for (const char* path : paths) {
        FILE* file = fopen(path, "rb");
        uint8_t actual[4]{};
        require(file && fread(actual, 1, sizeof(actual), file) == 3 && memcmp(actual, bad, 3) == 0,
                "Habits blocked save changed source");
        require(fclose(file) == 0, "Habits preserved fixture close");
      }
    }
    memcpy(game.bytes, good, sizeof(good));
    game.put32(4, UINT32_MAX);
    game.put32(423, game.checksum(game.bytes, 423));
    writeSlot(0, game.bytes, sizeof(game.bytes));
    {
      HabitTrackerActivity exhausted(renderer, mappedInputManager);
      exhausted.load();
      exhausted.dirty = true;
      require(!exhausted.blocked && exhausted.generation == UINT32_MAX && !exhausted.save() && exhausted.dirty,
              "Habits generation exhaustion");
    }
    require(remove(paths[0]) == 0 && remove(paths[1]) == 0, "Habits fixture cleanup");
    LOG_INF("GAMETEST", "GAME TEST RESULT: PASS habits");
  }

  static void eventLogger() {
    EventLoggerActivity game(renderer, mappedInputManager);
    game.entries = makeUniqueNoThrow<EventLoggerActivity::Entry[]>(game.MAX_ENTRIES);
    require(bool(game.entries), "Event logger test allocation");
    // Native fixture writes stay inside the runner's disposable filesystem.
    const char* path = "fs_/crossink/logs/events.csv";
    auto fixture = [&](const char* bytes, size_t length) {
      FILE* file = fopen(path, "wb");
      require(file != nullptr, "Event logger fixture open");
      require(fwrite(bytes, 1, length, file) == length, "Event logger fixture write");
      require(fclose(file) == 0, "Event logger fixture close");
      game.loadEntries();
    };
    FILE* file = fopen(path, "wb");
    require(file != nullptr, "Event logger ring fixture open");
    for (int i = 0; i < 55; ++i) require(fprintf(file, "%d,note%d\n", i, i) > 0, "Event logger ring fixture write");
    require(fclose(file) == 0, "Event logger ring fixture close");
    game.loadEntries();
    require(!game.storageError && game.count == 50 && game.entry(0).uptime == 54 && game.entry(49).uptime == 5,
            "Event logger newest-first ring retention");
    game.saveEntry("new,note");
    require(!game.storageError && game.count == 50 && strcmp(game.entry(0).text, "new,note") == 0 &&
                game.entry(49).uptime == 6,
            "Event logger append reload");
    const char* invalid[] = {"1,torn", "4294967296,overflow\n", "-1,negative\n", "1,\n", ",text\n", "1,text\r\n"};
    for (const char* text : invalid) {
      const size_t length = strlen(text);
      fixture(text, length);
      require(game.storageError, "Event logger accepted corrupt record");
      game.saveEntry("must not append");
      file = fopen(path, "rb");
      require(file != nullptr, "Event logger preserved file open");
      char actual[64]{};
      const size_t read = fread(actual, 1, sizeof(actual), file);
      require(fclose(file) == 0, "Event logger preserved file close");
      require(read == length && memcmp(actual, text, length) == 0, "Event logger changed corrupt file");
    }
    const char nul[] = "1,te\0xt\n";
    fixture(nul, sizeof(nul) - 1);
    require(game.storageError, "Event logger embedded NUL accepted");
    fixture("4294967295,max\n", strlen("4294967295,max\n"));
    require(!game.storageError && game.count == 1 && game.entry(0).uptime == UINT32_MAX, "Event logger max timestamp");
    LOG_INF("GAMETEST", "GAME TEST RESULT: PASS event_logger ring append malformed preservation");
  }

  static void flashcards() {
    FlashcardActivity game(renderer, mappedInputManager);
    game.data = makeUniqueNoThrow<FlashcardActivity::DeckData>();
    require(bool(game.data), "Flashcard test allocation");
    // Native paths refer only to the runner's disposable simulator filesystem.
    require(rename("fs_/crossink/flashcards", "fs_/crossink/decks_saved") == 0, "Flashcard fixture backup");
    game.deckCount = 7;
    game.loadError = game.scanLimited = true;
    game.scanDecks();
    require(game.deckCount == 0 && !game.loadError && !game.scanLimited, "Flashcard missing directory clears state");
    require(rename("fs_/crossink/scan_file", "fs_/crossink/flashcards") == 0, "Flashcard file-path fixture");
    game.scanDecks();
    require(game.deckCount == 0 && game.loadError && !game.scanLimited, "Flashcard non-directory rejection");
    require(rename("fs_/crossink/flashcards", "fs_/crossink/scan_file") == 0, "Flashcard file-path restore");
    for (unsigned count : {0u, 511u, 512u, 513u}) {
      char path[64];
      snprintf(path, sizeof(path), "fs_/crossink/scan%u", count);
      require(rename(path, "fs_/crossink/flashcards") == 0, "Flashcard scan-limit fixture");
      game.scanDecks();
      require(game.deckCount == 0 && !game.loadError && game.scanLimited == (count >= 512),
              "Flashcard ignored-entry work-limit boundary");
      require(rename("fs_/crossink/flashcards", path) == 0, "Flashcard scan-limit restore");
    }
    require(rename("fs_/crossink/decks_saved", "fs_/crossink/flashcards") == 0, "Flashcard deck fixtures restore");
    game.scanDecks();
    require(!game.loadError && !game.scanLimited && game.deckCount == 11,
            "Flashcard directory extension/name/directory filtering");
    for (int i = 0; i < game.deckCount; ++i) {
      require(strcmp(game.data->names[i], "directory.csv") != 0 && strlen(game.data->names[i]) < 64,
              "Flashcard scanner accepted directory or oversized name");
      for (int j = 0; j < i; ++j)
        require(strcmp(game.data->names[i], game.data->names[j]) != 0, "Flashcard scanner duplicated entry");
    }
    for (unsigned i = 0; i < 8; ++i) {
      char path[96];
      snprintf(path, sizeof(path), "fs_/crossink/flashcards/extra%u.csv", i);
      FILE* file = fopen(path, "wb");
      require(file != nullptr, "Flashcard scan fixture creation");
      require(fputs("a,b", file) >= 0 && fclose(file) == 0, "Flashcard scan fixture write");
    }
    game.scanDecks();
    require(!game.loadError && game.scanLimited && game.deckCount == game.MAX_DECKS,
            "Flashcard scanner capacity limit");
    for (unsigned i = 0; i < 8; ++i) {
      char path[96];
      snprintf(path, sizeof(path), "fs_/crossink/flashcards/extra%u.csv", i);
      require(remove(path) == 0, "Flashcard scan fixture removal");
    }
    game.scanDecks();
    require(!game.loadError && !game.scanLimited && game.deckCount == 11,
            "Flashcard scanner retry clears capacity flag");
    auto load = [&](const char* name) {
      snprintf(game.data->names[0], sizeof(game.data->names[0]), "%s", name);
      return game.loadDeck();
    };
    require(load("valid.csv") && game.cardCount == 2, "Flashcard CRLF/blank/final-line parsing");
    require(strcmp(game.data->cards[0].front, "front") == 0 && strcmp(game.data->cards[0].back, "back") == 0 &&
                strcmp(game.data->cards[1].back, "answer,comma") == 0,
            "Flashcard field content");
    require(load("max.csv") && game.cardCount == 32, "Flashcard maximum deck");
    for (int i = 0; i < game.cardCount; ++i)
      require(strlen(game.data->cards[i].front) == 127 && strlen(game.data->cards[i].back) == 127 &&
                  game.data->cards[i].correct == 0 && game.data->cards[i].wrong == 0,
              "Flashcard field limit or score reset");
    const char* invalid[] = {"empty.csv",      "no_comma.csv",  "no_front.csv", "no_back.csv",  "nul.csv",
                             "long_front.csv", "long_back.csv", "too_many.csv", "oversize.csv", "missing.csv"};
    for (const auto* name : invalid)
      require(!load(name) && game.loadError && game.cardCount == 0, "Flashcard invalid deck retained usable cards");
    require(load("valid.csv") && !game.loadError && game.cardCount == 2, "Flashcard retry after invalid deck");
    LOG_INF("GAMETEST", "GAME TEST RESULT: PASS flashcards valid limits malformed retry");
  }

  static void snake() {
    SnakeActivity game(renderer, mappedInputManager);
    game.snake = makeUniqueNoThrow<SnakeActivity::Point[]>(game.MAX_CELLS);
    require(bool(game.snake), "Snake test allocation");
    game.gridW = 32;
    game.gridH = 48;
    game.snakeLength = 3;
    game.snake[0] = {3, 2};
    game.snake[1] = {2, 2};
    game.snake[2] = {1, 2};
    game.food = {10, 10};
    game.step();
    require(game.snakeLength == 3 && game.snake[0].x == 4 && game.snake[2].x == 2 && game.score == 0,
            "Snake ordinary movement");
    game.food = {5, 2};
    game.step();
    require(game.snakeLength == 4 && game.snake[0].x == 5 && game.snake[3].x == 2 && game.score == 10,
            "Snake growth score or retained tail");
    require(!game.isSnakeAt(game.food.x, game.food.y), "Snake food overlaps body");
    game.snake[0] = {1, 1};
    game.snake[1] = {1, 2};
    game.snake[2] = {0, 2};
    game.snake[3] = {0, 1};
    game.nextDirX = -1;
    game.food = {10, 10};
    game.step();
    require(game.state == SnakeActivity::PLAYING && game.snake[0].x == 0, "Snake moving-tail collision");
    game.step();
    require(game.state == SnakeActivity::GAME_OVER, "Snake wall collision");
    game.state = SnakeActivity::PLAYING;
    game.snake[0] = {1, 1};
    game.snake[1] = {1, 2};
    game.snake[2] = {0, 2};
    game.snake[3] = {0, 1};
    game.nextDirX = 0;
    game.nextDirY = 1;
    game.step();
    require(game.state == SnakeActivity::GAME_OVER && game.snake[0].y == 1, "Snake body collision");
    game.state = SnakeActivity::PLAYING;
    game.snakeLength = game.MAX_CELLS - 1;
    for (int i = 0; i < game.snakeLength; ++i)
      game.snake[i] = {static_cast<int16_t>(i % 32), static_cast<int16_t>(i / 32)};
    for (unsigned seed = 1; seed <= 32; ++seed) {
      game.rngState = seed;
      game.spawnFood();
      require(game.food.x == 31 && game.food.y == 47, "Snake final free food cell");
    }
    game.snake[game.snakeLength++] = {31, 47};
    game.spawnFood();
    require(game.state == SnakeActivity::GAME_OVER, "Snake full-board completion");
    LOG_INF("GAMETEST", "GAME TEST RESULT: PASS snake movement growth tail collisions food full");
  }

  static void maze() {
    MazeActivity game(renderer, mappedInputManager);
    // Reuse the same fallible storage as production; never put 12 KB on the stack.
    game.data = makeUniqueNoThrow<MazeActivity::MazeStorage>();
    require(bool(game.data), "Maze test storage allocation");
    for (int size = 0; size < 3; ++size) {
      game.mazeW = game.SIZES_W[size];
      game.mazeH = game.SIZES_H[size];
      for (unsigned seed = 1; seed <= 16; ++seed) {
        game.rngState = seed;
        game.generateMaze();
        int edges = 0;
        for (int y = 0; y < game.mazeH; ++y) {
          for (int x = 0; x < game.mazeW; ++x) {
            const auto walls = game.data->maze[y][x];
            require(game.isVisited(x, y), "Maze generation missed a cell");
            if (x == 0) require(walls & 8, "Maze west boundary open");
            if (y == 0) require(walls & 1, "Maze north boundary open");
            if (x == game.mazeW - 1) require(walls & 2, "Maze east boundary open");
            if (y == game.mazeH - 1) require(walls & 4, "Maze south boundary open");
            if (x + 1 < game.mazeW) {
              require(bool(walls & 2) == bool(game.data->maze[y][x + 1] & 8), "Maze east/west mismatch");
              edges += !(walls & 2);
            }
            if (y + 1 < game.mazeH) {
              require(bool(walls & 4) == bool(game.data->maze[y + 1][x] & 1), "Maze north/south mismatch");
              edges += !(walls & 4);
            }
          }
        }
        require(edges == game.mazeW * game.mazeH - 1, "Maze spanning tree edge count");
        game.startSolving();
        unsigned steps = 0;
        while (game.state == MazeActivity::SOLVING && steps++ < 4800) game.solveStep();
        require(game.state == MazeActivity::SOLVE_DONE && game.solvePathFound, "Maze solver completion");
        require(game.solvePathLen > 0 && game.solvePathLen <= game.MAX_PATH, "Maze path bound");
        require(game.data->work[0] == 0 && game.data->work[game.solvePathLen - 1] == game.mazeW * game.mazeH - 1,
                "Maze path endpoints");
        for (int i = 1; i < game.solvePathLen; ++i) {
          const int a = game.data->work[i - 1], b = game.data->work[i];
          require(b >= 0 && b < game.mazeW * game.mazeH, "Maze path cell bound");
          const int ax = a % game.mazeW, ay = a / game.mazeW, bx = b % game.mazeW, by = b / game.mazeW;
          require(abs(ax - bx) + abs(ay - by) == 1, "Maze path nonadjacent step");
          const int wall = bx > ax ? 2 : bx < ax ? 8 : by > ay ? 4 : 1;
          require(!(game.data->maze[ay][ax] & wall), "Maze solution crosses wall");
        }
      }
    }
    memset(game.data->maze, 15, sizeof(game.data->maze));
    game.startSolving();
    game.solveStep();
    require(game.state == MazeActivity::SOLVE_DONE && !game.solvePathFound, "Maze unreachable exit");
    LOG_INF("GAMETEST", "GAME TEST RESULT: PASS maze boards=48 walls paths unreachable");
  }

  static void minesweeper() {
    // Native-only activity on the host stack; production storage is unchanged.
    MinesweeperActivity game(renderer, mappedInputManager);
    for (int difficulty = 0; difficulty < 3; ++difficulty) {
      game.cols = difficulty == 0 ? 8 : 10;
      game.rows = difficulty == 0 ? 12 : 16;
      game.mineCount = difficulty == 0 ? 10 : difficulty == 1 ? 25 : 40;
      for (unsigned seed = 1; seed <= 32; ++seed) {
        game.rngState = seed;
        game.initGame();
        const int sx = seed % game.cols, sy = seed % game.rows;
        game.placeMines(sx, sy);
        unsigned mines = 0;
        for (int x = 0; x < game.cols; ++x) {
          for (int y = 0; y < game.rows; ++y) {
            if (game.isMine(x, y)) {
              ++mines;
              require(abs(x - sx) > 1 || abs(y - sy) > 1, "Minesweeper first-reveal safety");
            } else {
              unsigned adjacent = 0;
              for (int mx = 0; mx < game.cols; ++mx)
                for (int my = 0; my < game.rows; ++my)
                  if (game.isMine(mx, my) && abs(mx - x) <= 1 && abs(my - y) <= 1) ++adjacent;
              require(game.getCellValue(x, y) == adjacent, "Minesweeper adjacent count");
            }
          }
        }
        require(mines == static_cast<unsigned>(game.mineCount), "Minesweeper mine count");
        require(!game.checkWin(), "Minesweeper unopened board won");
        game.reveal(sx, sy);
        for (int x = 0; x < game.cols; ++x)
          for (int y = 0; y < game.rows; ++y) {
            require(!game.isMine(x, y) || !game.isRevealed(x, y), "Minesweeper flood revealed mine");
            if (!game.isMine(x, y)) game.reveal(x, y);
          }
        require(game.checkWin(), "Minesweeper revealed safe cells not won");
      }
    }
    game.initGame();
    game.grid[0][0] = MinesweeperActivity::FLAGGED;
    game.reveal(-1, 0);
    game.reveal(game.cols, game.rows);
    game.reveal(0, 0);
    require(!game.isRevealed(0, 0), "Minesweeper direct reveal ignored flag");
    game.reveal(5, 5);
    for (int x = 0; x < game.cols; ++x)
      for (int y = 0; y < game.rows; ++y)
        require(game.isRevealed(x, y) == (x != 0 || y != 0), "Minesweeper full flood or flag protection");
    require(!game.checkWin(), "Minesweeper flagged safe cell counted as revealed");
    game.grid[0][0] &= ~MinesweeperActivity::FLAGGED;
    game.reveal(0, 0);
    require(game.checkWin(), "Minesweeper empty-board reveal incomplete");
    LOG_INF("GAMETEST", "GAME TEST RESULT: PASS minesweeper boards=96 counts safety flood flags win");
  }

  static void tetris() {
    // Native simulator stack only; no shipping allocation changes.
    TetrisActivity game(renderer, mappedInputManager);
    for (int piece = 0; piece < 7; ++piece) {
      for (int rotation = 0; rotation < 4; ++rotation) {
        unsigned cells = 0;
        for (int row = 0; row < 4; ++row)
          for (int col = 0; col < 4; ++col) cells += game.getPieceBit(game.PIECES[piece].shape[rotation], row, col);
        require(cells == 4, "Tetris rotation must contain four cells");
        require(game.canPlace(piece, rotation, 3, 0), "Tetris empty-board spawn rejected");
        require(!game.canPlace(piece, rotation, -4, 0), "Tetris left boundary missed");
        require(!game.canPlace(piece, rotation, 10, 0), "Tetris right boundary missed");
        require(!game.canPlace(piece, rotation, 3, 20), "Tetris floor boundary missed");
      }
    }
    game.board[1][3] = 1;
    require(!game.canPlace(0, 0, 3, 0), "Tetris occupied-cell collision missed");
    memset(game.board, 0, sizeof(game.board));
    require(game.clearLines() == 0, "Tetris empty board cleared a line");
    for (int count = 1; count <= 4; ++count) {
      memset(game.board, 0, sizeof(game.board));
      game.board[19 - count][2] = 1;
      for (int row = 20 - count; row < 20; ++row) memset(game.board[row], 1, 10);
      require(game.clearLines() == count, "Tetris adjacent line clear count");
      for (int row = 0; row < 20; ++row)
        for (int col = 0; col < 10; ++col)
          require(game.board[row][col] == (row == 19 && col == 2), "Tetris line compaction lost cells");
    }
    memset(game.board, 0, sizeof(game.board));
    for (int row = 16; row < 20; ++row) {
      memset(game.board[row], 1, 10);
      game.board[row][5] = 0;
    }
    game.currentPiece = 0;
    game.currentRotation = 1;
    game.pieceX = 3;
    game.pieceY = 16;
    game.linesCleared = 9;
    game.level = 1;
    game.score = 0;
    game.nextPiece = 1;
    game.step();
    require(game.score == 800 && game.linesCleared == 13 && game.level == 2,
            "Tetris four-line score or level transition");
    require(game.currentPiece == 1 && game.state == TetrisActivity::PLAYING, "Tetris next piece did not spawn");
    for (const auto& row : game.board)
      for (auto cell : row) require(cell == 0, "Tetris four-line clear left cells");
    require(game.getDropInterval() == 730, "Tetris level-two drop interval");
    game.level = 100;
    require(game.getDropInterval() == 100, "Tetris drop interval minimum");
    memset(game.board, 1, sizeof(game.board));
    game.spawnPiece();
    require(game.state == TetrisActivity::GAME_OVER, "Tetris blocked spawn must end game");
    LOG_INF("GAMETEST", "GAME TEST RESULT: PASS tetris shapes collisions lines scoring level spawn");
  }

  static void chess() {
    ChessActivity game(renderer, mappedInputManager);
    game.initBoard();
    unsigned moves = 0;
    for (int r = 0; r < 8; ++r)
      for (int c = 0; c < 8; ++c) {
        if (!game.isOwnPiece(game.board[r][c])) continue;
        game.computeValidMoves(r, c);
        moves += game.validMoves.used;
      }
    require(moves == 20, "Chess initial legal move count differs");
    memset(game.board, 0, sizeof(game.board));
    game.board[7][4] = ChessActivity::W_KING;
    game.board[6][4] = ChessActivity::W_ROOK;
    game.board[0][4] = ChessActivity::B_ROOK;
    game.board[0][0] = ChessActivity::B_KING;
    require(game.wouldBeInCheck(6, 4, 6, 5), "Pinned rook exposes king");
    require(!game.wouldBeInCheck(6, 4, 5, 4), "Pinned rook may stay on pin line");
    require(game.board[6][4] == ChessActivity::W_ROOK && game.board[6][5] == ChessActivity::EMPTY,
            "Legality probe modified board");
    require(game.wouldBeInCheck(6, 4, 0, 0), "King capture allowed");
    memset(game.board, 0, sizeof(game.board));
    game.board[7][4] = ChessActivity::W_KING;
    game.board[5][4] = ChessActivity::B_KING;
    require(game.wouldBeInCheck(7, 4, 6, 4), "Kings may not become adjacent");
    memset(game.board, 0, sizeof(game.board));
    game.board[6][4] = ChessActivity::W_KING;
    game.board[0][0] = ChessActivity::B_KING;
    game.board[4][3] = ChessActivity::B_PAWN;
    require(game.wouldBeInCheck(6, 4, 5, 4), "Pawn diagonal attack missed");
    require(!game.wouldBeInCheck(6, 4, 5, 3), "Pawn forward move treated as capture");
    memset(game.board, 0, sizeof(game.board));
    game.board[7][4] = ChessActivity::W_KING;
    game.board[0][7] = ChessActivity::B_KING;
    game.board[1][0] = ChessActivity::W_PAWN;
    game.doMove(1, 0, 0, 0);
    require(game.board[0][0] == ChessActivity::W_QUEEN, "Pawn promotion failed");
    memset(game.board, 0, sizeof(game.board));
    game.whiteTurn = false;
    game.board[0][0] = ChessActivity::B_KING;
    game.board[2][2] = ChessActivity::W_KING;
    game.board[1][1] = ChessActivity::W_QUEEN;
    game.checkGameState();
    require(game.gameOver && game.inCheck && !game.hasAnyLegalMove(), "Checkmate not detected");
    game.gameOver = false;
    game.board[1][1] = ChessActivity::EMPTY;
    game.board[1][2] = ChessActivity::W_QUEEN;
    game.checkGameState();
    require(game.gameOver && !game.inCheck && !game.hasAnyLegalMove(), "Stalemate not detected");
    memset(game.board, 0, sizeof(game.board));
    game.whiteTurn = false;
    game.gameOver = false;
    game.board[0][0] = ChessActivity::B_KING;
    game.board[7][7] = ChessActivity::W_KING;
    game.board[3][3] = ChessActivity::B_ROOK;
    game.board[3][5] = ChessActivity::W_QUEEN;
    const int capture = game.scoreBotMove(3, 3, 3, 5);
    require(capture > game.scoreBotMove(3, 3, 3, 4), "Chess bot ignores free queen");
    require(game.board[3][3] == ChessActivity::B_ROOK && game.board[3][5] == ChessActivity::W_QUEEN && !game.whiteTurn,
            "Chess bot scoring changed board or turn");
    bool botDone = false;
    for (unsigned step = 0; step < 130 && !botDone; ++step) {
      botDone = game.botMove();
      if (!botDone)
        require(
            game.board[3][3] == ChessActivity::B_ROOK && game.board[3][5] == ChessActivity::W_QUEEN && !game.whiteTurn,
            "Chess search yield changes position");
    }
    require(botDone && game.botEvaluated <= 128 && !game.botSearching, "Chess bot search bound");
    require(game.board[3][5] == ChessActivity::B_ROOK && game.whiteTurn, "Chess bot failed free queen capture");
    memset(game.board, 0, sizeof(game.board));
    game.whiteTurn = false;
    game.board[0][0] = ChessActivity::B_KING;
    game.board[7][7] = ChessActivity::W_KING;
    game.board[3][3] = ChessActivity::B_QUEEN;
    game.board[3][5] = ChessActivity::W_PAWN;
    game.board[6][5] = ChessActivity::W_ROOK;
    require(game.scoreBotMove(3, 3, 3, 5) < game.scoreBotMove(3, 3, 3, 2), "Chess bot ignores exposed queen");
    game.board[6][1] = ChessActivity::B_PAWN;
    require(game.scoreBotMove(6, 1, 7, 1) >= 800 && game.board[6][1] == ChessActivity::B_PAWN &&
                game.board[7][1] == ChessActivity::EMPTY,
            "Chess bot promotion scoring/restoration");
    memset(game.board, 0, sizeof(game.board));
    game.board[0][0] = ChessActivity::B_KING;
    game.board[7][7] = ChessActivity::W_KING;
    game.board[3][3] = ChessActivity::B_QUEEN;
    game.board[3][5] = ChessActivity::W_ROOK;
    game.board[6][0] = ChessActivity::B_ROOK;
    game.board[6][2] = ChessActivity::W_PAWN;
    require(game.scoreBotMove(6, 0, 6, 2) > game.scoreBotMove(3, 3, 2, 3), "Chess tactic fixture one-ply bias");
    require(game.scoreBotMove(6, 0, 6, 2, true) < game.scoreBotMove(3, 3, 2, 3, true),
            "Chess reply search misses hanging queen elsewhere");
    require(game.board[3][3] == ChessActivity::B_QUEEN && game.board[6][0] == ChessActivity::B_ROOK &&
                game.board[6][2] == ChessActivity::W_PAWN && !game.whiteTurn,
            "Chess reply search fails board/turn restoration");
    memset(game.board, 0, sizeof(game.board));
    game.board[0][0] = ChessActivity::B_KING;
    game.board[2][2] = ChessActivity::W_KING;
    game.board[2][1] = ChessActivity::W_QUEEN;
    game.whiteTurn = true;
    require(game.scoreBotMove(2, 1, 1, 1, true) == 30000, "Chess search misses mate in one");
    require(game.scoreBotMove(2, 1, 1, 2, true) == 0, "Chess search misses stalemate");

    memset(game.board, 0, sizeof(game.board));
    game.whiteTurn = true;
    game.board[7][4] = ChessActivity::W_KING;
    game.board[0][0] = ChessActivity::B_KING;
    game.board[3][4] = ChessActivity::W_PAWN;
    game.board[1][3] = ChessActivity::B_PAWN;
    game.doMove(1, 3, 3, 3);
    game.computeValidMoves(3, 4);
    bool hasEp = false;
    for (auto [r, c] : game.validMoves) hasEp |= r == 2 && c == 3;
    require(hasEp && !game.wouldBeInCheck(3, 4, 2, 3), "Chess legal en passant missing");
    game.scoreBotMove(3, 4, 2, 3);
    require(game.board[3][3] == ChessActivity::B_PAWN && game.enPassantRow == 2 && game.enPassantCol == 3,
            "Chess en passant scoring state restoration");
    game.board[0][4] = ChessActivity::B_ROOK;
    require(game.wouldBeInCheck(3, 4, 2, 3), "Chess en passant exposing king allowed");
    game.board[0][4] = ChessActivity::EMPTY;
    game.doMove(3, 4, 2, 3);
    require(game.board[3][3] == ChessActivity::EMPTY && game.board[2][3] == ChessActivity::W_PAWN &&
                game.enPassantRow == -1,
            "Chess en passant execution or expiry");
    game.doMove(2, 3, 1, 3);
    require(game.enPassantRow == -1, "Chess ordinary move created en passant");
    memset(game.board, 0, sizeof(game.board));
    game.whiteTurn = false;
    game.board[0][0] = ChessActivity::B_KING;
    game.board[7][7] = ChessActivity::W_KING;
    game.board[4][4] = ChessActivity::B_PAWN;
    game.board[6][3] = ChessActivity::W_PAWN;
    game.doMove(6, 3, 4, 3);
    game.computeValidMoves(4, 4);
    hasEp = false;
    for (auto [r, c] : game.validMoves) hasEp |= r == 5 && c == 3;
    require(hasEp, "Chess black en passant missing");
    game.doMove(0, 0, 0, 1);
    game.computeValidMoves(4, 4);
    for (auto [r, c] : game.validMoves) require(r != 5 || c != 3, "Chess expired en passant still legal");
    memset(game.board, 0, sizeof(game.board));
    game.board[3][3] = ChessActivity::B_PAWN;
    game.whiteTurn = true;
    require(game.isSquareAttacked(4, 2, false) && game.isSquareAttacked(4, 4, false),
            "Black pawn empty diagonal attacks");
    require(!game.isSquareAttacked(4, 3, false) && game.whiteTurn, "Black pawn forward attack or turn mutation");
    game.board[3][3] = ChessActivity::W_PAWN;
    game.whiteTurn = false;
    require(game.isSquareAttacked(2, 2, true) && game.isSquareAttacked(2, 4, true),
            "White pawn empty diagonal attacks");
    require(!game.isSquareAttacked(2, 3, true) && !game.whiteTurn, "White pawn forward attack or turn mutation");
    for (bool white : {true, false}) {
      const int home = white ? 7 : 0, enemy = white ? 0 : 7;
      const auto king = white ? ChessActivity::W_KING : ChessActivity::B_KING;
      const auto rook = white ? ChessActivity::W_ROOK : ChessActivity::B_ROOK;
      auto setupCastle = [&]() {
        memset(game.board, 0, sizeof(game.board));
        game.whiteTurn = white;
        game.castleRights = 15;
        game.board[home][4] = king;
        game.board[home][0] = game.board[home][7] = rook;
        game.board[enemy][4] = white ? ChessActivity::B_KING : ChessActivity::W_KING;
      };
      setupCastle();
      require(game.canCastle(true) && game.canCastle(false), "Chess open castling lanes");
      game.scoreBotMove(home, 4, home, 6);
      require(game.castleRights == 15 && game.board[home][7] == rook && game.board[home][5] == ChessActivity::EMPTY,
              "Chess castling score restoration");
      game.board[home][1] = rook;
      require(!game.canCastle(false) && game.canCastle(true), "Chess queenside b-file obstruction");
      setupCastle();
      game.board[enemy][5] = white ? ChessActivity::B_ROOK : ChessActivity::W_ROOK;
      require(!game.canCastle(true), "Chess castle through check");
      setupCastle();
      game.board[enemy][6] = white ? ChessActivity::B_ROOK : ChessActivity::W_ROOK;
      require(!game.canCastle(true), "Chess castle into check");
      setupCastle();
      game.board[enemy][4] = white ? ChessActivity::B_ROOK : ChessActivity::W_ROOK;
      require(!game.canCastle(true) && !game.canCastle(false), "Chess castle out of check");
      setupCastle();
      game.doMove(home, 7, home - (white ? 1 : -1), 7);
      game.doMove(home - (white ? 1 : -1), 7, home, 7);
      require(!game.canCastle(true) && game.canCastle(false), "Chess rook return restored rights");
      setupCastle();
      game.doMove(home, 4, home, 5);
      game.doMove(home, 5, home, 4);
      require(!game.canCastle(true) && !game.canCastle(false), "Chess king return restored rights");
      setupCastle();
      game.board[enemy][7] = white ? ChessActivity::B_ROOK : ChessActivity::W_ROOK;
      game.doMove(enemy, 7, home, 7);
      game.board[home][7] = rook;  // A replacement rook must not restore the right.
      require(!game.canCastle(true), "Chess rook capture retained castling rights");
      for (bool kingSide : {true, false}) {
        setupCastle();
        game.doMove(home, 4, home, kingSide ? 6 : 2);
        require(game.board[home][kingSide ? 6 : 2] == king && game.board[home][kingSide ? 5 : 3] == rook &&
                    game.board[home][kingSide ? 7 : 0] == ChessActivity::EMPTY &&
                    game.board[home][4] == ChessActivity::EMPTY,
                "Chess castle execution");
      }
    }
    memset(game.board, 0, sizeof(game.board));
    game.board[0][0] = ChessActivity::B_KING;
    game.board[7][7] = ChessActivity::W_KING;
    require(game.insufficientMaterial(), "Chess bare kings draw");
    game.board[3][3] = ChessActivity::W_KNIGHT;
    require(game.insufficientMaterial(), "Chess lone knight draw");
    game.board[4][4] = ChessActivity::B_KNIGHT;
    require(!game.insufficientMaterial(), "Chess two knights incorrectly declared dead");
    game.board[3][3] = ChessActivity::W_BISHOP;
    game.board[4][4] = ChessActivity::B_BISHOP;
    require(game.insufficientMaterial(), "Chess same-color bishops draw");
    game.board[4][4] = ChessActivity::EMPTY;
    game.board[4][3] = ChessActivity::B_BISHOP;
    require(!game.insufficientMaterial(), "Chess opposite-color bishops incorrectly declared dead");
    game.board[4][3] = ChessActivity::B_PAWN;
    require(!game.insufficientMaterial(), "Chess pawn position incorrectly declared dead");
    game.board[4][3] = ChessActivity::EMPTY;
    game.whiteTurn = true;
    game.gameOver = false;
    game.checkGameState();
    require(game.gameOver && game.state == ChessActivity::GAME_OVER, "Chess material draw not applied");
    for (bool white : {true, false}) {
      for (unsigned choice = 0; choice < 4; ++choice) {
        memset(game.board, 0, sizeof(game.board));
        game.whiteTurn = white;
        game.gameOver = false;
        game.vsBot = false;
        game.board[0][7] = ChessActivity::B_KING;
        game.board[7][7] = ChessActivity::W_KING;
        game.selectedY = white ? 1 : 6;
        game.selectedX = 0;
        game.cursorY = white ? 0 : 7;
        game.cursorX = 0;
        game.board[game.selectedY][0] = white ? ChessActivity::W_PAWN : ChessActivity::B_PAWN;
        game.state = ChessActivity::PROMOTION;
        game.promotionChoice = choice;
        game.finishHumanMove();
        const uint8_t expected[] = {ChessActivity::W_QUEEN, ChessActivity::W_ROOK, ChessActivity::W_BISHOP,
                                    ChessActivity::W_KNIGHT};
        require(game.board[game.cursorY][0] == expected[choice] + (white ? 0 : 6) && game.whiteTurn != white &&
                    game.board[white ? 1 : 6][0] == ChessActivity::EMPTY,
                "Chess chosen promotion/turn completion");
      }
    }
    game.initBoard();
    game.gameOver = false;
    game.botThinking = false;
    game.state = ChessActivity::SELECT_PIECE;
    require(game.positionCount == 1 && game.repetitions == 1, "Chess initial history");
    uint8_t initialPosition[34], changedPosition[34];
    game.encodePosition(initialPosition);
    game.whiteTurn = false;
    game.encodePosition(changedPosition);
    require(memcmp(initialPosition, changedPosition, 34) != 0, "Chess repetition turn identity");
    game.whiteTurn = true;
    game.castleRights = 0;
    game.encodePosition(changedPosition);
    require(memcmp(initialPosition, changedPosition, 34) != 0, "Chess repetition castling identity");
    game.initBoard();
    for (unsigned cycle = 0; cycle < 4; ++cycle) {
      const int moves[4][4] = {{7, 6, 5, 5}, {0, 6, 2, 5}, {5, 5, 7, 6}, {2, 5, 0, 6}};
      for (const auto& move : moves) {
        game.doMove(move[0], move[1], move[2], move[3]);
        game.whiteTurn = !game.whiteTurn;
        game.recordPosition();
        game.checkGameState();
      }
      require(game.repetitions == cycle + 2, "Chess repeated knight cycle count");
      require(game.gameOver == (cycle == 3), "Chess fivefold automatic boundary");
      if (cycle == 1) {
        require(game.claimDraw(), "Chess threefold claim");
        game.gameOver = false;
        game.state = ChessActivity::SELECT_PIECE;
      }
    }
    game.doMove(6, 0, 5, 0);
    game.whiteTurn = false;
    game.recordPosition();
    require(game.positionCount == 1 && game.repetitions == 1, "Chess irreversible history reset");
    // Exact position identity ignores pinned/unavailable en passant captures.
    memset(game.board, 0, sizeof(game.board));
    game.board[7][4] = ChessActivity::W_KING;
    game.board[0][0] = ChessActivity::B_KING;
    game.board[3][4] = ChessActivity::W_PAWN;
    game.board[3][3] = ChessActivity::B_PAWN;
    game.board[0][4] = ChessActivity::B_ROOK;
    game.whiteTurn = true;
    game.enPassantRow = game.enPassantCol = -1;
    game.encodePosition(initialPosition);
    game.enPassantRow = 2;
    game.enPassantCol = 3;
    game.encodePosition(changedPosition);
    require(memcmp(initialPosition, changedPosition, 34) == 0, "Chess pinned en passant identity");
    game.board[0][4] = ChessActivity::EMPTY;
    game.encodePosition(changedPosition);
    require(changedPosition[33] == 4, "Chess legal en passant identity");
    game.enPassantCol = 0;
    game.encodePosition(changedPosition);
    require(changedPosition[33] == 0, "Chess unavailable en passant identity");
    game.initBoard();
    require(game.quietHalfmoves == 0, "Chess new game move counter");
    game.state = ChessActivity::SELECT_PIECE;
    game.gameOver = false;
    game.botThinking = false;
    game.quietHalfmoves = 99;
    require(!game.claimDraw(), "Chess premature 50-move claim");
    game.quietHalfmoves = 100;
    game.state = ChessActivity::SELECT_TARGET;
    game.validMoves.clear();
    require(!game.claimDraw(), "Chess claim on invalid intended target");
    game.state = ChessActivity::SELECT_PIECE;
    game.botThinking = true;
    require(!game.claimDraw(), "Chess claim during bot turn");
    game.botThinking = false;
    require(game.claimDraw() && game.gameOver && game.state == ChessActivity::GAME_OVER, "Chess valid 50-move claim");
    require(!game.claimDraw(), "Chess repeat claim after game over");
    game.gameOver = false;
    game.state = ChessActivity::SELECT_PIECE;
    game.quietHalfmoves = 99;
    game.selectedY = 7;
    game.selectedX = 1;
    game.cursorY = 5;
    game.cursorX = 2;
    game.computeValidMoves(7, 1);
    game.state = ChessActivity::SELECT_TARGET;
    require(game.intendedDrawReason() == 1 && game.quietHalfmoves == 99 && game.whiteTurn &&
                game.board[7][1] == ChessActivity::W_KNIGHT && game.board[5][2] == ChessActivity::EMPTY &&
                game.positionCount == 1 && game.castleRights == 15,
            "Chess intended fifty-move claim/restoration");
    require(game.claimDraw() && game.gameOver, "Chess intended claim completion");
    game.initBoard();
    game.gameOver = false;
    game.state = ChessActivity::SELECT_TARGET;
    game.quietHalfmoves = 99;
    game.selectedY = 6;
    game.selectedX = 0;
    game.cursorY = 5;
    game.cursorX = 0;
    game.computeValidMoves(6, 0);
    require(!game.claimDraw() && game.board[6][0] == ChessActivity::W_PAWN, "Chess pawn intended claim rejection");
    game.state = ChessActivity::SELECT_PIECE;
    game.quietHalfmoves = 100;
    game.botThinking = true;
    require(game.botMove() && game.gameOver && !game.botThinking && !game.botSearching && game.positionCount == 1,
            "Chess bot current-position claim");
    game.initBoard();
    game.gameOver = false;
    game.state = ChessActivity::SELECT_PIECE;
    // Two complete knight cycles except the final return: declare that return.
    for (unsigned i = 0; i < 7; ++i) {
      const int moves[4][4] = {{7, 6, 5, 5}, {0, 6, 2, 5}, {5, 5, 7, 6}, {2, 5, 0, 6}};
      const auto& move = moves[i % 4];
      game.doMove(move[0], move[1], move[2], move[3]);
      game.whiteTurn = !game.whiteTurn;
      game.recordPosition();
    }
    game.selectedY = 2;
    game.selectedX = 5;
    game.cursorY = 0;
    game.cursorX = 6;
    game.computeValidMoves(2, 5);
    game.state = ChessActivity::SELECT_TARGET;
    require(game.intendedDrawReason() == 2 && !game.whiteTurn && game.positionCount == 8 &&
                game.board[2][5] == ChessActivity::B_KNIGHT && game.board[0][6] == ChessActivity::EMPTY,
            "Chess intended threefold claim/restoration");
    require(game.claimDraw() && game.gameOver, "Chess intended threefold completion");
    game.initBoard();
    game.gameOver = false;
    game.state = ChessActivity::SELECT_PIECE;
    game.quietHalfmoves = 149;
    game.scoreBotMove(7, 1, 5, 2);
    require(game.quietHalfmoves == 149, "Chess bot probe changes move counter");
    game.doMove(7, 1, 5, 2);
    require(game.quietHalfmoves == 150, "Chess quiet move counter");
    game.whiteTurn = false;
    game.gameOver = false;
    game.checkGameState();
    require(game.gameOver, "Chess automatic 75-move draw missing");
    game.doMove(1, 0, 2, 0);
    require(game.quietHalfmoves == 0, "Chess pawn move counter reset");
    game.quietHalfmoves = 149;
    game.board[4][4] = ChessActivity::B_BISHOP;
    game.board[3][3] = ChessActivity::W_ROOK;
    game.doMove(4, 4, 3, 3);
    require(game.quietHalfmoves == 0, "Chess capture counter reset");
    memset(game.board, 0, sizeof(game.board));
    game.board[0][0] = ChessActivity::B_KING;
    game.board[2][2] = ChessActivity::W_KING;
    game.board[1][1] = ChessActivity::W_QUEEN;
    game.quietHalfmoves = 150;
    game.whiteTurn = false;
    game.gameOver = false;
    game.checkGameState();
    require(game.gameOver && game.inCheck && strcmp(game.gameOverMsg, tr(STR_CHESS_MATE)) == 0,
            "Chess mate must precede 75-move draw");
    // Exercise the coordinate invariant the analyzer loses across reply generation.
    for (unsigned piece = ChessActivity::W_PAWN; piece <= ChessActivity::B_KING; ++piece)
      for (int square = 0; square < 64; ++square) {
        memset(game.board, 0, sizeof(game.board));
        game.board[square / 8][square % 8] = piece;
        game.whiteTurn = game.isWhite(piece);
        game.castleRights = 0;
        game.enPassantRow = game.enPassantCol = -1;
        ChessActivity::MoveList candidates;
        game.addCandidates(square / 8, square % 8, candidates);
        for (const auto& [row, col] : candidates)
          require(row >= 0 && row < 8 && col >= 0 && col < 8, "Chess candidate coordinate invariant");
      }
    // Simulator-only boundary fixtures: a search yield must preserve all
    // persistent position state; budget expiry must commit exactly one move.
    for (bool timeLimit : {false, true}) {
      game.initBoard();
      game.gameOver = game.botThinking = game.botSearching = false;
      game.state = ChessActivity::SELECT_PIECE;
      uint8_t boardBefore[8][8];
      memcpy(boardBefore, game.board, sizeof(boardBefore));
      require(!game.botMove(), "Chess opening search should yield");
      require(memcmp(boardBefore, game.board, sizeof(boardBefore)) == 0 && game.whiteTurn && game.quietHalfmoves == 0 &&
                  game.castleRights == 15 && game.enPassantRow == -1 && game.enPassantCol == -1 &&
                  game.positionCount == 1 && game.repetitions == 1,
              "Chess search yield corrupts persistent position");
      if (timeLimit)
        game.botSearchStart = millis() - 2001;
      else
        game.botEvaluated = 128;
      const unsigned evaluated = game.botEvaluated;
      require(game.botMove() && !game.botSearching && game.botEvaluated == evaluated && !game.whiteTurn &&
                  memcmp(boardBefore, game.board, sizeof(boardBefore)) != 0 && game.positionCount <= 2,
              "Chess search cap must commit without evaluating another root");
    }
    game.initBoard();
    game.positionCount = 150;
    game.quietHalfmoves = 150;
    game.recordPosition();
    require(game.positionCount == 151, "Chess history final bounded slot");
    game.initBoard();
    require(game.positionCount == 1 && game.repetitions == 1 && game.quietHalfmoves == 0,
            "Chess new-game history reset after capacity");
    LOG_INF("GAMETEST", "GAME TEST RESULT: PASS chess initial pin king pawn promotion mate stalemate");
  }

 public:
  static void run() {
    const uint8_t natural[] = {1, 13}, soft[] = {1, 1, 9}, hard[] = {1, 1, 9, 10}, bust[] = {13, 12, 2};
    require(CasinoActivity::handValue(natural, 2) == 21, "Blackjack natural value");
    require(CasinoActivity::handValue(soft, 3) == 21, "Blackjack soft aces");
    require(CasinoActivity::handValue(hard, 4) == 21, "Blackjack ace demotion");
    require(CasinoActivity::handValue(bust, 3) == 22, "Blackjack bust value");
    LOG_INF("GAMETEST", "GAME TEST RESULT: PASS blackjack hand values");
    eventLogger();
    habits();
    transit();
    casinoPersistence();
    casinoGameplay();
    casinoSettlement();
    flashcards();
    snake();
    maze();
    minesweeper();
    tetris();
    chess();
    SudokuActivity game(renderer, mappedInputManager);
    // Fixed small host-only snapshots; no framebuffer or production allocation.
    uint8_t clues[9][9]{};
    for (unsigned seed = 1; seed <= 32; ++seed) {
      game.rngState = seed;
      game.generatePuzzle();
      memcpy(clues, game.board, sizeof(clues));
      require(game.validateBoard(game.board, false), "Generated clues conflict");
      unsigned givens = 0;
      for (unsigned r = 0; r < 9; ++r)
        for (unsigned c = 0; c < 9; ++c) {
          require(game.fixed[r][c] == (game.board[r][c] != 0), "Fixed clue mask differs");
          givens += game.fixed[r][c];
        }
      require(givens == 30, "Generated clue count differs from 30");
      solve(game);
      require(game.state == SudokuActivity::SOLVED && game.validateBoard(game.board, true),
              "Generated puzzle not solved correctly");
      for (unsigned r = 0; r < 9; ++r)
        for (unsigned c = 0; c < 9; ++c)
          require(!clues[r][c] || clues[r][c] == game.board[r][c], "Solver altered a given clue");
    }
    memset(game.board, 0, sizeof(game.board));
    require(!game.validateBoard(game.board, true), "Empty board counted as complete");
    solve(game);
    require(game.state == SudokuActivity::SOLVED && game.validateBoard(game.board, true), "Empty puzzle solve failed");
    for (unsigned kind = 0; kind < 4; ++kind) {
      memset(game.board, 0, sizeof(game.board));
      game.board[0][0] = kind == 3 ? 10 : 1;
      if (kind == 0) game.board[0][8] = 1;
      if (kind == 1) game.board[8][0] = 1;
      if (kind == 2) game.board[1][1] = 1;
      memcpy(clues, game.board, sizeof(clues));
      solve(game);
      require(game.state == SudokuActivity::NO_SOLUTION, "Invalid clues were accepted");
      require(!memcmp(clues, game.board, sizeof(clues)), "Failure changed the player's board");
    }
    memset(game.board, 0, sizeof(game.board));
    for (unsigned c = 0; c < 8; ++c) game.board[0][c] = c + 1;
    game.board[1][8] = 9;  // Row 0 needs 9 in column 8, but that column already has it.
    require(game.validateBoard(game.board, false), "Unsatisfiable fixture has conflicting clues");
    memcpy(clues, game.board, sizeof(clues));
    solve(game);
    require(game.state == SudokuActivity::NO_SOLUTION, "Unsatisfiable board not detected");
    require(!memcmp(clues, game.board, sizeof(clues)), "Unsolvable input changed");
    LOG_INF("GAMETEST", "GAME TEST RESULT: PASS sudoku seeds=32 empty=1 invalid=4 unsatisfiable=1");
    std::_Exit(0);
  }
};
void runSimulatorGameTestTick() {
  if (std::getenv("CROSSINK_SIMULATOR_GAME_TEST")) SimulatorGameTest::run();
}
#endif
