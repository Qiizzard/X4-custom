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
        if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8 && isEnemyPiece(board[nr][nc])) {
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
  board[tx][ty] = savedSource;
  board[fx][fy] = EMPTY;

  int kx, ky;
  const bool found = findKing(whiteTurn, kx, ky);
  if (!found) LOG_ERR("CHESS", "Missing own king");
  bool check = !found || isSquareAttacked(kx, ky, !whiteTurn);

  // Undo
  board[fx][fy] = savedSource;
  board[tx][ty] = savedTarget;
  return check;
}

void ChessActivity::computeValidMoves(int fx, int fy) {
  validMoves.clear();
  MoveList pseudo;
  addMovesForPiece(fx, fy, pseudo);
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
      addMovesForPiece(r, c, pseudo);
      for (auto& [tr, tc] : pseudo) {
        if (!wouldBeInCheck(r, c, tr, tc)) return true;
      }
    }
  }
  return false;
}

void ChessActivity::doMove(int fx, int fy, int tx, int ty) {
  uint8_t p = board[fx][fy];
  board[tx][ty] = p;
  board[fx][fy] = EMPTY;

  // Pawn promotion to queen
  if ((p == W_PAWN && tx == 0) || (p == B_PAWN && tx == 7)) {
    board[tx][ty] = isWhite(p) ? W_QUEEN : B_QUEEN;
  }
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
  }
}

void ChessActivity::botMove() {
  int chosenR = 0, chosenC = 0, chosenToR = 0, chosenToC = 0;
  unsigned seen = 0;
  for (int r = 0; r < 8; ++r)
    for (int c = 0; c < 8; ++c) {
      if (!isOwnPiece(board[r][c])) continue;
      MoveList targets;
      addMovesForPiece(r, c, targets);
      for (const auto& [tr, tc] : targets) {
        if (wouldBeInCheck(r, c, tr, tc)) continue;
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

void ChessActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      (botThinking && mappedInput.wasReleased(MappedInputManager::Button::Back))) {
    finish();
    return;
  }
  RenderLock lock(*this);
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
        doMove(selectedY, selectedX, cursorY, cursorX);
        whiteTurn = !whiteTurn;
        state = SELECT_PIECE;
        validMoves.clear();
        checkGameState();
        // Trigger bot move
        if (vsBot && !gameOver) {
          botThinking = true;
          botThinkStart = millis();
          requestUpdate();
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
    if (gameOver) GUI.drawPopup(renderer, gameOverMsg);
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), gameOver ? tr(STR_NEW_GAME) : tr(STR_CONFIRM),
                                            tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
