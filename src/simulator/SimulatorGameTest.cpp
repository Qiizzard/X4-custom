#ifdef SIMULATOR
#include "SimulatorGameTest.h"

#include <Logging.h>

#include <cstdlib>
#include <cstring>

#include "activities/apps/sudoku/SudokuActivity.h"
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

 public:
  static void run() {
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
