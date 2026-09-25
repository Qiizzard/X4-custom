# Casino — core modes, partial source port, unverified

Adapts Biscuit's Coin Flip (2x total return), Higher/Lower (ties win, integer
1.5x pot growth) and single-zero Roulette (2x/3x/36x returns). Seven fixed stakes
span 10–1,000. Bankroll starts at 1,000 each new entry; credits and pot cap at
1,000,000 with widened arithmetic before clamping. Reset requires confirmation.
Higher/Lower cashes out on Confirm or Back; ranks 1–13 use a shuffled fixed
52-card rank deck, reshuffled when exhausted and before a round if nearly empty.

Play credits only: no cash value, purchases, cash transactions or prizes.
No `casino.dat` or other storage is read/written. Saved progress, Slots,
Blackjack and Loot Box remain pending. Cosmetic xorshift and modulo selection
are not cryptographic or claimed perfectly uniform. No real-money suitability.

Fixed scalar state and 52-byte deck; no move/card vectors, growing strings,
per-frame allocations, extra framebuffer, rapid animation or auto-sleep override.
Translated controls show choices/stakes and limitations. Session-only means
exiting discards the bankroll; this is disclosed in the menu.

Deferred V1/device: all roulette bet boundaries (especially zero), exact-number
selection, outcomes/returns, insufficient funds, caps/reset cancel, Higher/Lower
ties, deck exhaustion, cash-out/Back, mode changes, orientations, sleep and
repeated entry/exit heap. No cache reset; gameplay tests not performed yet.
