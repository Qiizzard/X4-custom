# Tetris — wip / unverified

Source: `biscuit-reference/src/activities/apps/TetrisActivity.{h,cpp}`,
MIT, Copyright (c) 2025 Dave Allie.

200-byte in-object board and flash-resident piece tables. Board dimensions follow available renderer space; there is no per-frame allocation. Rotation, ghost piece, next piece, soft/hard drop, scoring and retries retained.
All three games use per-instance clock-seeded xorshift for game randomness,
not cryptographic entropy. User labels use translations with English fallback.
These are button-operated X3/X4 ports; touch interaction remains deferred.

Validation: C3 compile sanity check only for P1; result recorded in the queue.
Host, simulator interaction, soak, runtime memory and physical display checks
are deferred to V1. No hardware pass is claimed.

Hardware path: Home → Tools → Games → Tetris. Verify directional controls,
confirm action, restart and Back return; play through score/win/loss transitions.
For Minesweeper test short reveal versus held Confirm flag and first-cell safety;
for Tetris test rotation, line clears and hard drop; for Snake test eating,
wall/self collision and tail movement. Inspect e-ink refresh/ghosting, both
orientations, free heap and largest allocatable block after repeated exits.
No EPUB cache reset is required.
