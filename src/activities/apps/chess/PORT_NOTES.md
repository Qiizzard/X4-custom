# Chess — simplified source port, unverified

Adapted from Biscuit's ChessActivity board setup, pseudo-legal move generation,
king-safety filtering, checkmate/stalemate and random-move opponent. This is not
full tournament chess: repetition,
50-move claims are absent, as disclosed in setup.
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
check escapes, mate/stalemate, automatic promotion, new game, Back during bot
wait, both orientations and repeated entry/exit heap/stack high-water marks.
Measure worst-position bot latency on C3. No cache reset required.

C1 first increment: no new heap allocations or activity fields. A 13-entry
constexpr material table and reversible board edits evaluate each legal move.
Device verification: play against the bot, offer a free queen, try a defended
pawn, and check response time and Back behavior on X4; measure stack high-water
and worst-position latency. Repetition/50-move claims and deeper search remain.
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
draw message and Confirm returns to setup; repetition/move-count remain pending.

Promotion picker uses mapped directions and Confirm; Back returns to target
selection without mutation. Both-color choice/turn-completion tests cover all
four pieces. Device: reach last rank, cycle Q/R/B/N, cancel then confirm; check
correct piece, turn, draw handling and bot response. No added heap allocation.

Automatic 75-move draw uses a saturating two-byte half-move counter, restored
by bot probes and reset by pawn moves/captures/new game. Mate/stalemate checks
run first. Device: confirm long quiet play ends with the translated draw result;
no history allocation. Fifty-move claims and repetition are still unsupported.
