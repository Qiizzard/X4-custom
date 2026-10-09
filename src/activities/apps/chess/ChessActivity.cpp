#include "ChessActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstring>

#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

void ChessActivity::initBoard() {
  castleRights = 15;
  enPassantRow = enPassantCol = -1;
  memset(board, EMPTY, sizeof(board));
  // Black pieces (top)
  board[0][0] = B_ROOK;
  board[0][1] = B_KNIGHT;
  board[0][2] = B_BISHOP;
  board[0][3] = B_QUEEN;
  board[0][4] = B_KING;
  board[0][5] = B_BISHOP;
  board[0][6] = B_KNIGHT;
  board[0][7] = B_ROOK;
  for (int c = 0; c < 8; c++) board[1][c] = B_PAWN;
  // White pieces (bottom)
  for (int c = 0; c < 8; c++) board[6][c] = W_PAWN;
  board[7][0] = W_ROOK;
  board[7][1] = W_KNIGHT;
  board[7][2] = W_BISHOP;
  board[7][3] = W_QUEEN;
  board[7][4] = W_KING;
  board[7][5] = W_BISHOP;
  board[7][6] = W_KNIGHT;
  board[7][7] = W_ROOK;
}

const char* ChessActivity::pieceChar(uint8_t piece) const {
  switch (piece) {
    case W_PAWN:
    case B_PAWN:
      return "P";
    case W_ROOK:
    case B_ROOK:
      return "R";
    case W_KNIGHT:
    case B_KNIGHT:
      return "N";
    case W_BISHOP:
    case B_BISHOP:
      return "B";
    case W_QUEEN:
    case B_QUEEN:
      return "Q";
    case W_KING:
    case B_KING:
      return "K";
    default:
      return "";
  }
}

void ChessActivity::addSlidingMoves(int fx, int fy, int dx, int dy, MoveList& moves) const {
  int x = fx + dx, y = fy + dy;
  while (x >= 0 && x < 8 && y >= 0 && y < 8) {
    if (board[x][y] == EMPTY) {
      moves.push_back({x, y});
    } else {
      if (isEnemyPiece(board[x][y])) moves.push_back({x, y});
      break;
    }
    x += dx;
    y += dy;
  }
}

void ChessActivity::addMovesForPiece(int fx, int fy, MoveList& moves) const {
  uint8_t p = board[fx][fy];
  int dir = isWhite(p) ? -1 : 1;

  switch (p) {
    case W_PAWN:
    case B_PAWN: {
      int startRow = isWhite(p) ? 6 : 1;
      // Forward
      if (fx + dir >= 0 && fx + dir < 8 && board[fx + dir][fy] == EMPTY) {
        moves.push_back({fx + dir, fy});
        // Double move from start
        if (fx == startRow && board[fx + 2 * dir][fy] == EMPTY) {
          moves.push_back({fx + 2 * dir, fy});
        }
      }
      // Captures
      for (int dc : {-1, 1}) {
        int nr = fx + dir, nc = fy + dc;
        if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8 &&
            (isEnemyPiece(board[nr][nc]) || (nr == enPassantRow && nc == enPassantCol && board[nr][nc] == EMPTY &&
                                             board[fx][nc] == (isWhite(p) ? B_PAWN : W_PAWN)))) {
          moves.push_back({nr, nc});
        }
      }
      break;
    }
    case W_ROOK:
    case B_ROOK:
      for (auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) addSlidingMoves(fx, fy, dx, dy, moves);
      break;
    case W_BISHOP:
    case B_BISHOP:
      for (auto [dx, dy] : {std::pair{1, 1}, {1, -1}, {-1, 1}, {-1, -1}}) addSlidingMoves(fx, fy, dx, dy, moves);
      break;
    case W_QUEEN:
    case B_QUEEN:
      for (auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}})
        addSlidingMoves(fx, fy, dx, dy, moves);
      break;
    case W_KNIGHT:
    case B_KNIGHT:
      for (auto [dx, dy] : {std::pair{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1}}) {
        int nr = fx + dx, nc = fy + dy;
        if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8 && !isOwnPiece(board[nr][nc])) {
          moves.push_back({nr, nc});
        }
      }
      break;
    case W_KING:
    case B_KING:
      for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
          if (dx == 0 && dy == 0) continue;
          int nr = fx + dx, nc = fy + dy;
          if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8 && !isOwnPiece(board[nr][nc])) {
            moves.push_back({nr, nc});
          }
        }
      }
      break;
    default:
      break;
  }
}

bool ChessActivity::isSquareAttacked(int tx, int ty, bool byWhite) {
  bool savedTurn = whiteTurn;
  whiteTurn = byWhite;
  MoveList moves;
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      uint8_t p = board[r][c];
      if (p == EMPTY) continue;
      if (byWhite ? !isWhite(p) : !isBlack(p)) continue;
      // Pawn attacks are diagonal even on empty squares; forward moves are not attacks.
      if (p == W_PAWN || p == B_PAWN) {
        if (tx == r + (byWhite ? -1 : 1) && std::abs(ty - c) == 1) {
          whiteTurn = savedTurn;
          return true;
        }
        continue;
      }
      moves.clear();
      addMovesForPiece(r, c, moves);
      for (auto& [mr, mc] : moves) {
        if (mr == tx && mc == ty) {
          whiteTurn = savedTurn;
          return true;
        }
      }
    }
  }
  whiteTurn = savedTurn;
  return false;
}

bool ChessActivity::findKing(bool white, int& kx, int& ky) const {
  uint8_t target = white ? W_KING : B_KING;
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      if (board[r][c] == target) {
        kx = r;
        ky = c;
        return true;
      }
    }
  }
  return false;
}

bool ChessActivity::wouldBeInCheck(int fx, int fy, int tx, int ty) {
  if (board[tx][ty] == W_KING || board[tx][ty] == B_KING) return true;
  // Simulate move
  uint8_t savedTarget = board[tx][ty];
  uint8_t savedSource = board[fx][fy];
  const bool enPassant = (savedSource == W_PAWN || savedSource == B_PAWN) && fy != ty && savedTarget == EMPTY &&
                         tx == enPassantRow && ty == enPassantCol;
  const uint8_t capturedPawn = board[fx][ty];
  if (enPassant) board[fx][ty] = EMPTY;
  const bool castle = (savedSource == W_KING || savedSource == B_KING) && fx == tx && std::abs(ty - fy) == 2;
  const int rookFrom = ty > fy ? 7 : 0, rookTo = ty > fy ? 5 : 3;
  const uint8_t rook = board[fx][rookFrom], transit = board[fx][rookTo];
  if (castle) {
    board[fx][rookTo] = rook;
    board[fx][rookFrom] = EMPTY;
  }
  board[tx][ty] = savedSource;
  board[fx][fy] = EMPTY;

  int kx, ky;
  const bool found = findKing(whiteTurn, kx, ky);
  if (!found) LOG_ERR("CHESS", "Missing own king");
  bool check = !found || isSquareAttacked(kx, ky, !whiteTurn);

  // Undo
  board[fx][fy] = savedSource;
  board[tx][ty] = savedTarget;
  if (enPassant) board[fx][ty] = capturedPawn;
  if (castle) {
    board[fx][rookFrom] = rook;
    board[fx][rookTo] = transit;
  }
  return check;
}

bool ChessActivity::canCastle(bool kingSide) {
  const int row = whiteTurn ? 7 : 0;
  const unsigned bit = (whiteTurn ? 0 : 2) + (kingSide ? 0 : 1);
  const int rookCol = kingSide ? 7 : 0;
  if (!(castleRights & (1u << bit)) || board[row][4] != (whiteTurn ? W_KING : B_KING) ||
      board[row][rookCol] != (whiteTurn ? W_ROOK : B_ROOK))
    return false;
  for (int col = kingSide ? 5 : 1; col < (kingSide ? 7 : 4); ++col)
    if (board[row][col] != EMPTY) return false;
  if (isSquareAttacked(row, 4, !whiteTurn)) return false;
  // Test transit with the king moved off its original square.
  if (wouldBeInCheck(row, 4, row, kingSide ? 5 : 3)) return false;
  return !wouldBeInCheck(row, 4, row, kingSide ? 6 : 2);
}

void ChessActivity::addCandidates(int row, int col, MoveList& moves) {
  addMovesForPiece(row, col, moves);
  if (row != (whiteTurn ? 7 : 0) || col != 4 || board[row][col] != (whiteTurn ? W_KING : B_KING)) return;
  if (canCastle(true)) moves.push_back({row, 6});
  if (canCastle(false)) moves.push_back({row, 2});
}

void ChessActivity::computeValidMoves(int fx, int fy) {
  validMoves.clear();
  MoveList pseudo;
  addCandidates(fx, fy, pseudo);
  for (auto& [tr, tc] : pseudo) {
    if (!wouldBeInCheck(fx, fy, tr, tc)) {
      validMoves.push_back({tr, tc});
    }
  }
}

bool ChessActivity::hasAnyLegalMove() {
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      if (!isOwnPiece(board[r][c])) continue;
      MoveList pseudo;
      addCandidates(r, c, pseudo);
      for (auto& [tr, tc] : pseudo) {
        if (!wouldBeInCheck(r, c, tr, tc)) return true;
      }
    }
  }
  return false;
}

void ChessActivity::doMove(int fx, int fy, int tx, int ty, unsigned promotion) {
  uint8_t p = board[fx][fy];
  const bool king = p == W_KING || p == B_KING;
  if (king) castleRights &= isWhite(p) ? ~3u : ~12u;
  // Moving from or capturing on a rook home square permanently revokes its right.
  for (int side = 0; side < 2; ++side) {
    const int row = side == 0 ? 7 : 0;
    if ((fx == row && fy == 7) || (tx == row && ty == 7)) castleRights &= ~(1u << (side * 2));
    if ((fx == row && fy == 0) || (tx == row && ty == 0)) castleRights &= ~(2u << (side * 2));
  }
  if (king && fx == tx && std::abs(ty - fy) == 2) {
    const int rookFrom = ty > fy ? 7 : 0, rookTo = ty > fy ? 5 : 3;
    board[fx][rookTo] = board[fx][rookFrom];
    board[fx][rookFrom] = EMPTY;
  }
  const bool pawn = p == W_PAWN || p == B_PAWN;
  if (pawn && fy != ty && board[tx][ty] == EMPTY && tx == enPassantRow && ty == enPassantCol) board[fx][ty] = EMPTY;
  enPassantRow = enPassantCol = -1;
  if (pawn && std::abs(tx - fx) == 2) {
    enPassantRow = (tx + fx) / 2;
    enPassantCol = fy;
  }
  board[tx][ty] = p;
  board[fx][fy] = EMPTY;

  // Human choice; bots and temporary evaluation default to queen.
  if ((p == W_PAWN && tx == 0) || (p == B_PAWN && tx == 7)) {
    static constexpr uint8_t pieces[] = {W_QUEEN, W_ROOK, W_BISHOP, W_KNIGHT};
    const auto promoted = pieces[promotion < 4 ? promotion : 0];
    board[tx][ty] = promoted + (isWhite(p) ? 0 : B_PAWN - W_PAWN);
  }
}

bool ChessActivity::insufficientMaterial() const {
  unsigned knights = 0, bishops = 0, kings = 0;
  int bishopColor = -1;
  for (int row = 0; row < 8; ++row) {
    for (int col = 0; col < 8; ++col) {
      const auto piece = board[row][col];
      if (piece == EMPTY) continue;
      if (piece == W_KING || piece == B_KING) {
        ++kings;
        continue;
      }
      if (piece == W_KNIGHT || piece == B_KNIGHT) {
        ++knights;
        continue;
      }
      if (piece != W_BISHOP && piece != B_BISHOP) return false;
      const int color = (row + col) & 1;
      if (bishopColor >= 0 && bishopColor != color) return false;
      bishopColor = color;
      ++bishops;
    }
  }
  return kings == 2 && ((knights == 0) || (knights == 1 && bishops == 0));
}

void ChessActivity::checkGameState() {
  int kx, ky;
  if (!findKing(whiteTurn, kx, ky)) {
    LOG_ERR("CHESS", "Missing king in game state");
    gameOver = true;
    state = GAME_OVER;
    gameOverMsg = tr(STR_CHESS_INVALID);
    return;
  }
  inCheck = isSquareAttacked(kx, ky, !whiteTurn);

  if (!hasAnyLegalMove()) {
    gameOver = true;
    state = GAME_OVER;
    gameOverMsg = inCheck ? tr(STR_CHESS_MATE) : tr(STR_CHESS_STALEMATE);
  } else if (insufficientMaterial()) {
    gameOver = true;
    state = GAME_OVER;
    gameOverMsg = tr(STR_CHESS_MATERIAL_DRAW);
  }
}

int ChessActivity::scoreBotMove(int fromRow, int fromCol, int toRow, int toCol) {
  // One-ply heuristic only. Fixed flash table and reversible board edits; no allocations.
  static constexpr int values[] = {0, 100, 500, 320, 330, 900, 20000, 100, 500, 320, 330, 900, 20000};
  const uint8_t source = board[fromRow][fromCol], target = board[toRow][toCol];
  const uint8_t savedRights = castleRights;
  const bool castle = (source == W_KING || source == B_KING) && fromRow == toRow && std::abs(toCol - fromCol) == 2;
  const int rookFrom = toCol > fromCol ? 7 : 0, rookTo = toCol > fromCol ? 5 : 3;
  const uint8_t rook = board[fromRow][rookFrom], transit = board[fromRow][rookTo];
  const int8_t savedEpRow = enPassantRow, savedEpCol = enPassantCol;
  const bool ep = (source == W_PAWN || source == B_PAWN) && fromCol != toCol && target == EMPTY &&
                  toRow == enPassantRow && toCol == enPassantCol;
  const uint8_t side = board[fromRow][toCol];
  int score = ep ? 100 : values[target];
  doMove(fromRow, fromCol, toRow, toCol);
  const uint8_t moved = board[toRow][toCol];
  score += values[moved] - values[source];  // Promotion gain.
  if (isSquareAttacked(toRow, toCol, !whiteTurn)) score -= values[moved];
  // Small positional tie-break: central squares and pawn advancement.
  score += 3 - std::min(std::abs(3 - toCol), std::abs(4 - toCol));
  if (source == W_PAWN) score += 6 - toRow;
  if (source == B_PAWN) score += toRow - 1;
  board[fromRow][fromCol] = source;
  board[toRow][toCol] = target;
  if (ep) board[fromRow][toCol] = side;
  if (castle) {
    board[fromRow][rookFrom] = rook;
    board[fromRow][rookTo] = transit;
  }
  castleRights = savedRights;
  enPassantRow = savedEpRow;
  enPassantCol = savedEpCol;
  return score;
}

void ChessActivity::botMove() {
  int chosenR = 0, chosenC = 0, chosenToR = 0, chosenToC = 0;
  unsigned seen = 0;
  int bestScore = -30000;
  for (int r = 0; r < 8; ++r)
    for (int c = 0; c < 8; ++c) {
      if (!isOwnPiece(board[r][c])) continue;
      MoveList targets;
      addCandidates(r, c, targets);
      for (const auto& [tr, tc] : targets) {
        if (wouldBeInCheck(r, c, tr, tc)) continue;
        const int score = scoreBotMove(r, c, tr, tc);
        if (score < bestScore) continue;
        if (score > bestScore) {
          bestScore = score;
          seen = 0;
        }
        if (randomValue() % ++seen == 0) {
          chosenR = r;
          chosenC = c;
          chosenToR = tr;
          chosenToC = tc;
        }
      }
    }
  if (!seen) {
    checkGameState();
    return;
  }
  doMove(chosenR, chosenC, chosenToR, chosenToC);
  whiteTurn = !whiteTurn;
  checkGameState();
}

void ChessActivity::onEnter() {
  Activity::onEnter();
  rng = millis() | 1u;
  state = SETUP;
  setupIndex = 0;
  vsBot = false;
  botThinking = false;
  requestUpdate();
}

void ChessActivity::finishHumanMove() {
  doMove(selectedY, selectedX, cursorY, cursorX, state == PROMOTION ? promotionChoice : 0);
  whiteTurn = !whiteTurn;
  state = SELECT_PIECE;
  validMoves.clear();
  checkGameState();
  if (vsBot && !gameOver) {
    botThinking = true;
    botThinkStart = millis();
  }
}

void ChessActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      (botThinking && mappedInput.wasReleased(MappedInputManager::Button::Back))) {
    finish();
    return;
  }
  RenderLock lock(*this);
  if (state == PROMOTION) {
    const unsigned previousChoice = promotionChoice;
    if (mappedInput.wasReleased(MappedInputManager::Button::Left) ||
        mappedInput.wasReleased(MappedInputManager::Button::Up))
      promotionChoice = (promotionChoice + 3) % 4;
    if (mappedInput.wasReleased(MappedInputManager::Button::Right) ||
        mappedInput.wasReleased(MappedInputManager::Button::Down))
      promotionChoice = (promotionChoice + 1) % 4;
    if (mappedInput.wasReleased(MappedInputManager::Button::Back))
      state = SELECT_TARGET;
    else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm))
      finishHumanMove();
    if (state != PROMOTION || promotionChoice != previousChoice) requestUpdate();
    return;
  }
  if (state == SETUP) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
        mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      setupIndex = 1 - setupIndex;
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      vsBot = (setupIndex == 1);
      initBoard();
      state = SELECT_PIECE;
      whiteTurn = true;
      inCheck = false;
      gameOver = false;
      cursorX = 4;
      cursorY = 7;
      validMoves.clear();
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
    }
    return;
  }

  // Bot thinking timer
  if (botThinking) {
    if (millis() - botThinkStart > 600) {
      botThinking = false;
      botMove();
      requestUpdate();
    }
    return;
  }

  if (state == GAME_OVER) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      state = SETUP;
      setupIndex = 0;
      vsBot = false;
      botThinking = false;
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
    }
    return;
  }

  bool moved = false;
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    if (cursorY > 0) cursorY--;
    moved = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    if (cursorY < 7) cursorY++;
    moved = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    if (cursorX > 0) cursorX--;
    moved = true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    if (cursorX < 7) cursorX++;
    moved = true;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (state == SELECT_PIECE) {
      if (isOwnPiece(board[cursorY][cursorX])) {
        selectedX = cursorX;
        selectedY = cursorY;
        computeValidMoves(cursorY, cursorX);
        if (!validMoves.empty()) {
          state = SELECT_TARGET;
        }
        moved = true;
      }
    } else if (state == SELECT_TARGET) {
      // Check if target is valid
      auto it = std::find(validMoves.begin(), validMoves.end(), std::pair<int, int>{cursorY, cursorX});
      if (it != validMoves.end()) {
        const auto piece = board[selectedY][selectedX];
        if ((piece == W_PAWN && cursorY == 0) || (piece == B_PAWN && cursorY == 7)) {
          promotionChoice = 0;
          state = PROMOTION;
        } else {
          finishHumanMove();
        }
      } else if (isOwnPiece(board[cursorY][cursorX])) {
        // Re-select different piece
        selectedX = cursorX;
        selectedY = cursorY;
        computeValidMoves(cursorY, cursorX);
        if (validMoves.empty()) {
          state = SELECT_PIECE;
        }
      } else {
        state = SELECT_PIECE;
        validMoves.clear();
      }
      moved = true;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (state == SELECT_TARGET) {
      state = SELECT_PIECE;
      validMoves.clear();
      moved = true;
    } else {
      finish();
      return;
    }
  }

  if (moved) requestUpdate();
}

void ChessActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware())
    TouchHeaderBackButton::draw(renderer, header, tr(STR_APP_CHESS), false);
  else
    GUI.drawHeader(renderer, header, tr(STR_APP_CHESS));
  const Rect area = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int line = renderer.getLineHeight(UI_10_FONT_ID) + 6;
  if (state == SETUP) {
    UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID, area.y + line,
                              setupIndex ? tr(STR_CHESS_BOT) : tr(STR_CHESS_HUMAN));
    UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID, area.y + 3 * line, tr(STR_CHESS_RULES));
    UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID, area.y + 4 * line, tr(STR_CHESS_DRAWS));
  } else {
    const int cell = std::max(1, std::min((area.width - 8) / 8, (area.height - 4 * line) / 8));
    const int left = area.x + (area.width - 8 * cell) / 2, top = area.y;
    for (int r = 0; r < 8; ++r)
      for (int c = 0; c < 8; ++c) {
        const int x = left + c * cell, y = top + r * cell;
        renderer.drawRect(x, y, cell, cell, true);
        const uint8_t piece = board[r][c];
        if (piece) {
          const bool black = isBlack(piece);
          if (black) renderer.fillRect(x + 3, y + 3, std::max(1, cell - 6), std::max(1, cell - 6), true);
          renderer.drawText(UI_10_FONT_ID, x + cell / 3, y + cell / 3, pieceChar(piece), !black);
        }
        if (r == cursorY && c == cursorX)
          renderer.drawRect(x + 1, y + 1, std::max(1, cell - 2), std::max(1, cell - 2), true);
        if (state == SELECT_TARGET && r == selectedY && c == selectedX)
          renderer.drawRect(x + 2, y + 2, std::max(1, cell - 4), std::max(1, cell - 4), true);
        if (std::find(validMoves.begin(), validMoves.end(), std::pair<int, int>{r, c}) != validMoves.end())
          renderer.fillRect(x + cell / 2, y + cell - 5, 3, 3, true);
      }
    const int y = top + 8 * cell + 4;
    UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID, y,
                              botThinking ? tr(STR_CHESS_THINKING)
                              : whiteTurn ? tr(STR_CHESS_WHITE)
                                          : tr(STR_CHESS_BLACK));
    if (inCheck) UITheme::drawCenteredText(renderer, area, UI_10_FONT_ID, y + line, tr(STR_CHESS_CHECK));
    if (state == PROMOTION) {
      static constexpr const char* choices[] = {"Q", "R", "B", "N"};
      char prompt[96];
      snprintf(prompt, sizeof(prompt), "%s: %s", tr(STR_CHESS_PROMOTION), choices[promotionChoice]);
      GUI.drawPopup(renderer, prompt);
    }
    if (gameOver) GUI.drawPopup(renderer, gameOverMsg);
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), gameOver ? tr(STR_NEW_GAME) : tr(STR_CONFIRM),
                                            tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
