# Snake — wip / unverified

Source: `biscuit-reference/src/activities/apps/SnakeActivity.{h,cpp}`,
MIT, Copyright (c) 2025 Dave Allie.

Body storage is one fallible 6144-byte allocation in onEnter, released in onExit; 32x48 maximum grid. No vector growth or second framebuffer. Food placement scans for a free cell and a full board ends the game. Moving into the vacated tail is allowed.
All three games use per-instance clock-seeded xorshift for game randomness,
not cryptographic entropy. User labels use translations with English fallback.
These are button-operated X3/X4 ports; touch interaction remains deferred.

Validation: C3 compile sanity check only for P1; result recorded in the queue.
Host, simulator interaction, soak, runtime memory and physical display checks
are deferred to V1. No hardware pass is claimed.

Hardware path: Home → Tools → Games → Snake. Verify directional controls,
confirm action, restart and Back return; play through score/win/loss transitions.
For Minesweeper test short reveal versus held Confirm flag and first-cell safety;
for Tetris test rotation, line clears and hard drop; for Snake test eating,
wall/self collision and tail movement. Inspect e-ink refresh/ghosting, both
orientations, free heap and largest allocatable block after repeated exits.
No EPUB cache reset is required.
