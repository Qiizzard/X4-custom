# Chess — simplified source port, unverified

Adapted from Biscuit's ChessActivity board setup, pseudo-legal move generation,
king-safety filtering, checkmate/stalemate and random-move opponent. This is not
full tournament chess: intended-move claims and bot draw claims remain absent.
The bot now scores legal moves for captures, promotion, exposed destination
pieces and small center/pawn-advance bonuses. This is a one-ply heuristic,
not multi-ply search or a rated engine; it can miss tactics elsewhere. Black pieces use a filled backing, white
pieces an unfilled backing; conventional P/R/N/B/Q/K labels identify pieces.

Each move list has 28 fixed pairs (228 bytes including count); a queen has at
most 27 pseudo-legal destinations. Nested legality checks use distinct bounded
local lists, no recursion or heap churn. The activity holds a 64-byte board and
one fixed valid-move list. Bot selection uses reservoir sampling among equally best-scoring legal moves
instead of the source's 218-entry/3,488-byte stack array. Cosmetic xorshift is
not cryptographic and modulo selection is not claimed perfectly uniform.

King captures are rejected before simulation; missing kings fail closed and
log instead of using uninitialized coordinates. All game mutations in loop
share the render lock. Back exits during the bot delay. Normal auto-sleep stays
available. UI uses runtime safe bounds and translations. No second framebuffer.

Deferred V1/device checks: Home → Tools → Games → Chess; human/bot setup, cursor
and reselection/cancel, blocked pawns, captures, pinned pieces, king adjacency,
check escapes, mate/stalemate, promotion choice, new game, Back during bot
wait, both orientations and repeated entry/exit heap/stack high-water marks.
Measure worst-position bot latency on C3. No cache reset required.

C1 first increment: no new heap allocations or activity fields. A 13-entry
constexpr material table and reversible board edits evaluate each legal move.
Device verification: play against the bot, offer a free queen, try a defended
pawn, and check response time and Back behavior on X4; measure stack high-water
and worst-position latency. Deeper search remains.
En passant tracks one eligible target until the next move. King-safety probes
remove and restore the captured pawn; bot probes also restore eligibility.
On device, test both colors, a missed one-turn opportunity and a pinned pawn.

Pawn attack detection treats empty diagonals as attacked and forward squares
as movement only. Simulator tests cover both colors and restored side-to-move.
Hardware: verify kings cannot enter pawn diagonals and can enter an otherwise
safe square directly ahead of a pawn. Castling now validates both lanes and king transit, tracks rights after king/rook
moves and rook captures, and moves/restores the rook in simulation and bot scoring.
Device: test both castles for both colors, occupied lanes, attacked transit squares
and a moved-and-returned rook. No new heap allocation; one byte tracks rights.

Insufficient-material detection covers bare kings, one lone knight/bishop,
and bishop-only positions on one square color. It is conservative, not a general
dead-position solver. On X4, verify an eligible capture ends with the translated
draw message and Confirm returns to setup.

Promotion picker uses mapped directions and Confirm; Back returns to target
selection without mutation. Both-color choice/turn-completion tests cover all
four pieces. Device: reach last rank, cycle Q/R/B/N, cancel then confirm; check
correct piece, turn, draw handling and bot response. No added heap allocation.

Automatic 75-move draw uses a saturating two-byte half-move counter, restored
by bot probes and reset by pawn moves/captures/new game. Mate/stalemate checks
run first. Device: confirm long quiet play ends with the translated draw result;
the move counter itself needs no history allocation.

Fifty-move claims are available on the current position after 100 quiet
half-moves: hold Confirm for 500 ms while choosing a piece. Target selection,
bot thinking and finished games reject claims. Uses the existing counter,
no new allocations. Intended-move claims (before the qualifying move) and bot
claims are not implemented. Device: play a qualifying quiet sequence, verify
the prompt, short Confirm still selects, and held Confirm ends in a draw;
verify pawn/capture resets remove eligibility. No cache reset required.

Repetition uses exact 34-byte packed positions (32 board, one turn/castling,
one legal en passant file), avoiding hash collisions. Only actual completed
moves record positions; probes do not. Pawn/capture moves clear history.
151 entries (5,134 bytes) cover the initial position and the 150 half-moves
before the automatic draw. Storage belongs to the activity allocated through
makeUniqueNoThrow; it is too large for stack and must not consume static RAM
while Chess is closed. No per-move allocation. Threefold enables held-Confirm
claims; fivefold ends automatically, after mate/stalemate checks.
Device: in two-player mode repeat Ng1-f3, Ng8-f6, Nf3-g1, Nf6-g8 twice;
claim prompt must appear. Continue to four cycles for automatic fivefold draw.
Verify Back/new game and repeated entry/exit heap stability. Hardware RAM/stack
and input timing remain unverified.

C3 compiled activity object measured 5,712 bytes on 2026-10-10; excludes
transient call stacks and renderer-owned memory. Firmware image 6,551,216 bytes
leaves 2,384 bytes in the unchanged OTA slot.
