# Minesweeper — wip / unverified

Source: `biscuit-reference/src/activities/apps/MinesweeperActivity.{h,cpp}`,
MIT, Copyright (c) 2025 Dave Allie.

160-byte in-object grid, a bounded 160-byte local flood-fill queue marking cells on enqueue, and a 96-byte header buffer. Flags placed before first reveal are preserved during mine placement. Difficulty selection draws bounded labels without std::string construction.
All three games use per-instance clock-seeded xorshift for game randomness,
not cryptographic entropy. User labels use translations with English fallback.
These are button-operated X3/X4 ports; touch interaction remains deferred.

Validation: C3 compile sanity check only for P1; result recorded in the queue.
Host, simulator interaction, soak, runtime memory and physical display checks
are deferred to V1. No hardware pass is claimed.

Hardware path: Home → Tools → Games → Minesweeper. Verify directional controls,
confirm action, restart and Back return; play through score/win/loss transitions.
For Minesweeper test short reveal versus held Confirm flag and first-cell safety;
for Tetris test rotation, line clears and hard drop; for Snake test eating,
wall/self collision and tail movement. Inspect e-ink refresh/ghosting, both
orientations, free heap and largest allocatable block after repeated exits.
No EPUB cache reset is required.
