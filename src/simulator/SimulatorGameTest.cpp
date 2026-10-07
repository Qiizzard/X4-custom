#ifdef SIMULATOR
#include "SimulatorGameTest.h"

#include <Logging.h>
#include <Memory.h>

#include <cstdlib>
#include <cstring>

#include "activities/apps/casino/CasinoActivity.h"
#include "activities/apps/chess/ChessActivity.h"
#include "activities/apps/event_logger/EventLoggerActivity.h"
#include "activities/apps/flashcards/FlashcardActivity.h"
#include "activities/apps/maze/MazeActivity.h"
#include "activities/apps/minesweeper/MinesweeperActivity.h"
#include "activities/apps/snake/SnakeActivity.h"
#include "activities/apps/sudoku/SudokuActivity.h"
#include "activities/apps/tetris/TetrisActivity.h"
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
    game.botMove();
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
