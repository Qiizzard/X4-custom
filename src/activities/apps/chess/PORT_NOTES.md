# Chess — simplified source port, unverified

Adapted from Biscuit's ChessActivity board setup, pseudo-legal move generation,
king-safety filtering, checkmate/stalemate and random-move opponent. This is not
a rated tournament engine; search and material-draw detection remain deliberately limited.
The bot now scores legal moves for captures, promotion, exposed destination
pieces and small center/pawn-advance bonuses, then subtracts the best evaluated
opponent reply. This is a capped two-ply heuristic, not a rated engine. Black pieces use a filled backing, white
pieces an unfilled backing; conventional P/R/N/B/Q/K labels identify pieces.

Each move list has 28 fixed pairs (228 bytes including count); a queen has at
most 27 pseudo-legal destinations. Nested legality checks use distinct bounded
local lists and no heap churn; reply evaluation nests one extra scoring frame. The activity holds a 64-byte board and
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
and worst-position latency.
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
half-moves: hold Confirm for 500 ms while choosing a piece. Bot thinking and finished games reject human claims. Uses the existing counter,
no new allocations. Intended-move claims are available by holding Confirm on a legal highlighted
quiet target. The bot accepts an eligible current-position draw before search. Device: play a qualifying quiet sequence, verify
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

C3 compiled activity object measured 5,736 bytes on 2026-10-10; excludes
transient call stacks and renderer-owned memory. Firmware image 6,552,304 bytes
leaves 1,296 bytes in the unchanged OTA slot.

Bounded reply search (2026-10-10): one root candidate per loop, maximum 128
root candidates; each reply scan checks at most 128 pseudo-moves and scores
at most 64 legal moves. The two-second deadline is checked between roots,
not a hard wall-clock guarantee. Back is handled before the next root. Board,
turn, en passant, castling and counter state are restored before yielding;
only the chosen move enters repetition history. Mate/stalemate at the reply
root are scored explicitly. Search order/caps can miss moves; draw history
is not evaluated in hypothetical positions. No per-node allocation.
Hardware: play against the bot, press Back during thinking, measure maximum
input latency and task stack high-water in crowded/promoted-piece positions.
Native tests cover a hanging piece elsewhere, mate/stalemate, bounded search
completion and unchanged board/turn at intermediate yields. Device checks
remain open; this does not establish a strength rating.

Intended claims (2026-10-10) use a 64-byte board snapshot plus a 34-byte
position key on the stack, restoring turn, rights, en passant and half-move
count without touching history. No new activity fields or allocation. Rejected
held-Confirm claims leave the move unplayed; short Confirm still plays it.
Hardware: after seven plies of the repeated-knight cycle, select Black's
f6-g8 return and hold Confirm; draw must end with the knight still on f6.
At 99 quiet half-moves, hold Confirm on a legal non-pawn, non-capture target;
verify unchanged board and draw result. Pawn/capture targets must not claim.
Bot policy accepts current-position claims immediately, including winning
positions; it does not assess whether playing on would be preferable.

V1 2026-10-10: native 50-cycle/10-minute entry-screen soak PASS (cycle heap
delta 0, idle +160 within 4096 slack). Search cap/yield/history boundary and
768 coordinate fixtures PASS. See V1_CHESS_RESOURCES_2026-10-10.json for C3
frame measurements and analyzer triage. Hardware validation is still open.
