#ifdef SIMULATOR
#include "SimulatorGameTest.h"

#include <Logging.h>

#include <cstdlib>
#include <cstring>

#include "activities/apps/casino/CasinoActivity.h"
#include "activities/apps/chess/ChessActivity.h"
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
