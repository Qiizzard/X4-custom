# Chess — simplified source port, unverified

Adapted from Biscuit's ChessActivity board setup, pseudo-legal move generation,
king-safety filtering, checkmate/stalemate and random-move opponent. This is not
full tournament chess: castling/en passant, underpromotion, repetition,
move-count and insufficient-material draws are absent, as disclosed in setup.
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
and worst-position latency. Castling/en passant/draw/promotion-choice work remains.
