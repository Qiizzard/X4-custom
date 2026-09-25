#pragma once
#include <Logging.h>

#include <cstdint>
#include <utility>

#include "activities/Activity.h"

class ChessActivity final : public Activity {
 public:
  explicit ChessActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Chess", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct MoveList {
    // A queen has at most 27 pseudo-legal destinations; no recursive search.
    std::pair<int, int> values[28];
    unsigned used = 0;
    void clear() { used = 0; }
    bool empty() const { return used == 0; }
    auto begin() { return values; }
    auto end() { return values + used; }
    void push_back(std::pair<int, int> value) {
      if (used < 28)
        values[used++] = value;
      else
        LOG_ERR("CHESS", "Move list overflow");
    }
  };
  uint32_t rng = 1;
  uint32_t randomValue() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
  }
  enum State { SETUP, SELECT_PIECE, SELECT_TARGET, GAME_OVER };
  enum Piece : uint8_t {
    EMPTY = 0,
    W_PAWN,
    W_ROOK,
    W_KNIGHT,
    W_BISHOP,
    W_QUEEN,
    W_KING,
    B_PAWN,
    B_ROOK,
    B_KNIGHT,
    B_BISHOP,
    B_QUEEN,
    B_KING
  };

  uint8_t board[8][8]{};
  int cursorX = 4, cursorY = 7;
  int selectedX = -1, selectedY = -1;
  State state = SELECT_PIECE;
  bool whiteTurn = true;
  bool inCheck = false;
  bool gameOver = false;
  const char* gameOverMsg = "";
  MoveList validMoves;

  // Bot
  bool vsBot = false;
  bool botThinking = false;
  unsigned long botThinkStart = 0;
  int setupIndex = 0;  // 0=vs Human, 1=vs Bot

  void botMove();
  void initBoard();
  bool isWhite(uint8_t piece) const { return piece >= W_PAWN && piece <= W_KING; }
  bool isBlack(uint8_t piece) const { return piece >= B_PAWN && piece <= B_KING; }
  bool isOwnPiece(uint8_t piece) const { return whiteTurn ? isWhite(piece) : isBlack(piece); }
  bool isEnemyPiece(uint8_t piece) const { return whiteTurn ? isBlack(piece) : isWhite(piece); }
  const char* pieceChar(uint8_t piece) const;

  void computeValidMoves(int fx, int fy);
  void addMovesForPiece(int fx, int fy, MoveList& moves) const;
  void addSlidingMoves(int fx, int fy, int dx, int dy, MoveList& moves) const;
  bool isSquareAttacked(int tx, int ty, bool byWhite);
  bool wouldBeInCheck(int fx, int fy, int tx, int ty);
  bool findKing(bool white, int& kx, int& ky) const;
  bool hasAnyLegalMove();
  void doMove(int fx, int fy, int tx, int ty);
  void checkGameState();
};
