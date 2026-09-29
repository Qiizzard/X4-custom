# Casino — core modes, partial source port, unverified

Adapts Biscuit's Coin Flip (2x total return), Higher/Lower (ties win, integer
1.5x pot growth) and single-zero Roulette (2x/3x/36x returns). Seven fixed stakes
span 10–1,000. Bankroll starts at 1,000 each new entry; credits and pot cap at
1,000,000 with widened arithmetic before clamping. Reset requires confirmation.
Higher/Lower cashes out on Confirm or Back; ranks 1–13 use a shuffled fixed
52-card rank deck, reshuffled when exhausted and before a round if nearly empty.

Play credits only: no cash value, purchases, cash transactions or prizes.
No `casino.dat` or other storage is read/written. Saved progress, additional slot machines/powerups,
and Loot Box remain pending. Cosmetic xorshift and modulo selection
are not cryptographic or claimed perfectly uniform. No real-money suitability.

Fixed scalar state and 52-byte deck; no move/card vectors, growing strings,
per-frame allocations, extra framebuffer, rapid animation or auto-sleep override.
Translated controls show choices/stakes and limitations. Session-only means
exiting discards the bankroll; this is disclosed in the menu.

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
rapid animation or storage writes. Maximum uncapped return is 50,000 with
the existing 1,000 maximum stake. Modulo selection has slight bias.
Deferred device checks: Casino -> Classic Slots, stake selection, all three
pair positions/triples, insufficient credits, cap, Back/Confirm navigation,
portrait/landscape layout and sleep/wake. No cache reset required.
