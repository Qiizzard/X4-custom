#ifdef SIMULATOR
#include "SimulatorGameOfLifeTest.h"

#include <Logging.h>
#include <Memory.h>

#include <cstring>

#include "activities/apps/game_of_life/GameOfLifeActivity.h"

bool verifySimulatorGameOfLifeRules(GfxRenderer& renderer, MappedInputManager& input) {
  // Isolated test instance: heap storage keeps the activity and boards off
  // the caller's stack. No onEnter(), rendering, or global activity changes.
  auto app = makeUniqueNoThrow<GameOfLifeActivity>(renderer, input);
  if (!app) return false;
  app->current = makeUniqueNoThrow<uint8_t[]>(app->kBufferBytes);
  app->next = makeUniqueNoThrow<uint8_t[]>(app->kBufferBytes);
  if (!app->current || !app->next) return false;
  app->boardReady = true;
  const auto clear = [&]() {
    std::memset(app->current.get(), 0, app->kBufferBytes);
    app->generation = 0;
    app->liveCount = 0;
  };
  const auto population = [&]() {
    uint32_t total = 0;
    for (size_t i = 0; i < app->kBufferBytes; ++i) {
      total += static_cast<uint32_t>(__builtin_popcount(app->current[i]));
    }
    return total;
  };
  clear();
  app->setCellAt(app->current.get(), 10, 10, true);
  app->setCellAt(app->current.get(), 11, 10, true);
  app->setCellAt(app->current.get(), 10, 11, true);
  app->setCellAt(app->current.get(), 11, 11, true);
  app->step();
  if (app->generation != 1 || app->liveCount != 4 || population() != 4 || !app->cellAt(app->current.get(), 10, 10) ||
      !app->cellAt(app->current.get(), 11, 10) || !app->cellAt(app->current.get(), 10, 11) ||
      !app->cellAt(app->current.get(), 11, 11))
    return false;

  clear();
  // Horizontal blinker spanning x=63/0 becomes vertical across y=127/0.
  app->setCellAt(app->current.get(), 63, 0, true);
  app->setCellAt(app->current.get(), 0, 0, true);
  app->setCellAt(app->current.get(), 1, 0, true);
  app->step();
  if (app->generation != 1 || app->liveCount != 3 || population() != 3 || !app->cellAt(app->current.get(), 0, 127) ||
      !app->cellAt(app->current.get(), 0, 0) || !app->cellAt(app->current.get(), 0, 1))
    return false;
  app->step();
  if (app->generation != 2 || app->liveCount != 3 || population() != 3 || !app->cellAt(app->current.get(), 63, 0) ||
      !app->cellAt(app->current.get(), 0, 0) || !app->cellAt(app->current.get(), 1, 0))
    return false;

  app->rngState = 1;
  app->seedRandom();
  if (app->generation != 0 || app->liveCount != population() || app->liveCount == 0) return false;
  LOG_INF("SMOKE", "Game of Life block, wrapped blinker and restart counters passed");
  return true;
}
#endif
