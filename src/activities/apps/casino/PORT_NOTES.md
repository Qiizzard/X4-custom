# Casino — core modes, partial source port, unverified

Adapts Biscuit's Coin Flip (2x total return), Higher/Lower (ties win, integer
1.5x pot growth) and single-zero Roulette (2x/3x/36x returns). Seven fixed stakes
span 10–1,000. Bankroll starts at 1,000 without a valid save; credits and pot cap at
1,000,000 with widened arithmetic before clamping. Reset requires confirmation.
Higher/Lower cashes out on Confirm or Back; ranks 1–13 use a shuffled fixed
52-card rank deck, reshuffled when exhausted and before a round if nearly empty.

Play credits only: no cash value, purchases, cash transactions or prizes.
Legacy Biscuit `casino.dat` is not imported. Additional slot machines/powerups remain pending. Cosmetic xorshift and modulo selection
are not cryptographic or claimed perfectly uniform. No real-money suitability.

Fixed scalar state and 52-byte deck; no move/card vectors, growing strings,
per-frame allocations, extra framebuffer, rapid animation or auto-sleep override.
Translated controls show choices/stakes and limitations. Explicit menu saves persist credits and collection; unsaved changes are discarded
on exit, as disclosed in the menu.

Deferred V1/device: all roulette bet boundaries (especially zero), exact-number
selection, outcomes/returns, insufficient funds, caps/reset cancel, Higher/Lower
ties, deck exhaustion, cash-out/Back, mode changes, orientations, sleep and
repeated entry/exit heap. No cache reset; gameplay tests not performed yet.

Blackjack adds two fixed 12-card rank arrays (24 bytes), a fresh deck per round,
ace adjustment, natural-blackjack precedence, pushes and dealer stand on soft
or hard 17. Natural pays 3:2 profit rounded down; other wins return 2x stake,
push returns stake. No split/double/insurance/surrender. Confirm hits; Right or
Back stands. Dealer hole-card total is withheld until settlement. Final tests
must cover both/player/dealer natural, multi-card 21, soft aces, busts, pushes,
odd-stake rounding, credit cap, repeated rounds and hidden dealer information.

Classic Slots adds three bytes of reel state and flash-resident symbol/payout
lookup tables. Six symbols follow the reference Classic machine: matching
triples return 50/20/10/8/5/3 times stake; any pair returns 2x; otherwise zero.
The deducted stake is included in these total returns. No holds/powerups,
rapid animation or automatic storage writes. Maximum uncapped return is 50,000 with
the existing 1,000 maximum stake. Modulo selection has slight bias.
Deferred device checks: Casino -> Classic Slots, stake selection, all three
pair positions/triples, insufficient credits, cap, Back/Confirm navigation,
portrait/landscape layout and sleep/wake. No cache reset required.

Loot Box preserves 50 reference collectibles and rarity boundaries (20 common,
15 rare, 10 epic, 5 legendary). Single costs 100; five draws cost 450. Base
rarity weights are 60/25/12/3; the fifth draw upgrades common to rare if the
first four had no rare-or-better item. Duplicates refund 25 immediately,
including duplicates within the same five-draw set. Collection uses 7 bytes;
results use five item IDs, five new/duplicate flags and two counters (12 bytes).
Item-name translation IDs live in a fixed constant table. No animation, icon
buffers or allocations. Unsaved collection changes reset on exit; bankroll reset alone
retains the current session collection, matching reference reset semantics.
Deferred V1/device: all rarity boundaries, five-draw guarantee, duplicate
refunds, collection wrap at 0/49, no-cost browsing with insufficient bankroll,
saved/unsaved re-entry, long translated labels/orientations and all six mode transitions.

Explicit Page Forward in the Casino menu saves credits and collection together
in alternating /crossink/casino-a.dat and casino-b.dat records. Each 23-byte
CAS1 record carries a nonwrapping generation and FNV-1a corruption checksum;
this is not authentication or anti-cheat. Load validates exact size, checksum,
credit cap, nonzero generation and reserved collection bits, choosing the
newest valid record. Both-invalid files block writes and remain preserved.
An unavailable card at entry disables saving until re-entry. Every file closes
explicitly; write, sync and close failures retain the previous active slot.
No save during an in-flight round, auto-save or legacy save import. A 23-byte
stack buffer is used per operation; no app-owned dynamic allocation is added.
Deferred device: explicit save/re-entry, unsaved exit, alternating-slot damage,
full/removed SD and interrupted write, no-write browsing, reset then save,
and collection/credits recovered together. Back up the two records before
intentional corruption tests; no EPUB cache reset needed.
