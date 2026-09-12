# Flashcards — wip / unverified

Adapted from Biscuit FlashcardActivity (MIT, Copyright 2025 Dave Allie).
One fallible lifecycle allocation contains 32 cards, 16 deck names and a line
buffer (9732 bytes). This is too large for the task stack and is released on
exit. No vector growth or per-render string allocation. Directory scanning is
bounded to 512 entries and 16 matching names; a visible limit notice appears.

Place lowercase `.csv` files in `/crossink/flashcards`. No automatic migration
from Biscuit's folder occurs. Format follows the simple original first-comma
separator: `front,back`, one card per line, max 127 bytes per side, 32 cards and
16 KiB per file. LF/CRLF and a final line without newline are accepted. Commas
in answers are retained; quoted CSV escaping is not supported. Malformed,
empty or oversized decks show an error instead of silently truncating cards.
Directory entries with names too long for the 63-byte name limit are skipped.
The app does not modify decks or persist scores; results describe one session.

Hardware: Tools → Flashcards, select deck, Confirm flips, Left marks wrong,
Right marks correct; Back returns to decks. Test cancel/empty directory, both
line endings, multibyte text, maximum fields/cards, malformed/oversized files,
more than 16 decks, all-answer result totals and repeated entry/exit while
checking free heap and largest block. Touch interaction remains deferred.
No EPUB cache reset is required.

Only C3 compile sanity is run in P3 (see queue). Host, simulator, parser,
soak and hardware validation remain deferred to V1.
