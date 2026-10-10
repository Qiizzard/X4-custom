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
#ifdef SIMULATOR
  friend class SimulatorGameTest;
#endif
  struct MoveList {
    // A queen has at most 27 pseudo-legal destinations; bounded search lists.
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
  enum State { SETUP, SELECT_PIECE, SELECT_TARGET, PROMOTION, GAME_OVER };
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
  // Exact compact positions since the last pawn move/capture. The automatic
  // 75-move draw bounds this at the initial position plus 150 half-moves.
  uint8_t positions[151][34]{};
  uint8_t positionCount = 0, repetitions = 1;
  uint16_t quietHalfmoves = 0;
  uint8_t castleRights = 0;  // White K/Q, Black K/Q; initialized with a new board.
  int8_t enPassantRow = -1, enPassantCol = -1;
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
  bool botSearching = false;
  uint8_t botSquare = 0, botTarget = 0, botChoice[4]{};
  unsigned botEvaluated = 0, botSeen = 0;
  int botBest = -32000;
  unsigned long botSearchStart = 0;
  unsigned promotionChoice = 0;
  int setupIndex = 0;  // 0=vs Human, 1=vs Bot

  int scoreBotMove(int fromRow, int fromCol, int toRow, int toCol, bool replies = false);
  int bestReplyScore();
  void finishHumanMove();
  bool claimDraw();
  void encodePosition(uint8_t (&position)[34]);
  void recordPosition();
  bool botMove();
  void initBoard();
  bool isWhite(uint8_t piece) const { return piece >= W_PAWN && piece <= W_KING; }
  bool isBlack(uint8_t piece) const { return piece >= B_PAWN && piece <= B_KING; }
  bool isOwnPiece(uint8_t piece) const { return whiteTurn ? isWhite(piece) : isBlack(piece); }
  bool isEnemyPiece(uint8_t piece) const { return whiteTurn ? isBlack(piece) : isWhite(piece); }
  const char* pieceChar(uint8_t piece) const;

  bool canCastle(bool kingSide);
  void addCandidates(int row, int col, MoveList& moves);
  void computeValidMoves(int fx, int fy);
  void addMovesForPiece(int fx, int fy, MoveList& moves) const;
  void addSlidingMoves(int fx, int fy, int dx, int dy, MoveList& moves) const;
  bool isSquareAttacked(int tx, int ty, bool byWhite);
  bool wouldBeInCheck(int fx, int fy, int tx, int ty);
  bool findKing(bool white, int& kx, int& ky) const;
  bool hasAnyLegalMove();
  void doMove(int fx, int fy, int tx, int ty, unsigned promotion = 0);
  bool insufficientMaterial() const;
  void checkGameState();
};
