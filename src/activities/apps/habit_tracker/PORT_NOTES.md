# Habit Tracker — wip / unverified

Adapted from Biscuit HabitTrackerActivity, MIT, Copyright 2025 Dave Allie.
Fixed ten-habit array and 427-byte serialization buffer live in the activity;
no persistent global buffer, raw-struct serialization or render-time allocation.
The shared keyboard is allocated fallibly only while adding a 31-byte name.

Sessions are explicit, not calendar days. Reopening preserves checks and never
advances the session. Page Forward offers a confirmation to finish the current
session. Checked habits extend their finished-session streak; unchecked habits
reset it. Toggling a check does not inflate the best streak. Right edits, Confirm
adds or requests deletion, and Back cancels confirmations. No RTC/GPS is faked.

Changes are saved after 750 ms idle, and before ordinary Back exit. Save failure
keeps dirty data for retry; forced/global exit or power loss can lose unsaved
changes. Two slots `/crossink/habits-a.dat` and `habits-b.dat` alternate: the
previous active slot is not overwritten by a new save. Load selects the highest
valid generation. Both invalid slots block edits and preserve files for repair.
No migration of Biscuit's unsafe raw structure file is attempted.

Format HBT1: 4-byte magic, LE32 generation, LE32 session, 1-byte count; ten
41-byte entries (32-byte NUL-terminated name, byte boolean, LE32 streak, LE32
best); LE32 FNV-1a checksum of the preceding 423 bytes. Exactly 427 bytes.
Checksum detects accidental corruption, not malicious changes. Writes, sync
and close are checked. This does not establish filesystem power-loss atomicity.

Hardware: Tools → Habit Tracker; add/edit/delete, toggle repeatedly, reopen,
finish sessions with different completed subsets and verify streak/best values.
Test missing/full SD, one/both corrupt slots and interrupted saves. Monitor
free heap/largest block across keyboard and entry/exit cycles. No EPUB cache
reset is required; touch controls are deferred. C3 compile sanity only for P3;
all parser, simulator, soak and device verification remain deferred to V1.
