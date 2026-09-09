# Maze — wip / unverified

Adapted from `biscuit-reference/src/activities/apps/MazeActivity.{h,cpp}`,
MIT, Copyright (c) 2025 Dave Allie.

A fallible onEnter allocation owns 12,302 bytes of bounded grid/visited/parent/
work storage, released in onExit. This storage is too large for the task stack
and need not remain resident while the app is closed. Generation reuses visited
bits and the work buffer; BFS later reuses that buffer, and path tracing reuses
it only after BFS stops. Supports Biscuit's 10x15, 20x30 and 40x60 grids without
a second framebuffer. The original separate path buffer and 4.8 KB local
generation stack are removed. Size selection uses bounded translated labels.

Hardware: Games → Maze. Up/Down selects size, Confirm generates; directions
move, Confirm animates a solution, Back stops solving or exits play. Verify all
three sizes, wall collisions, exit detection, regeneration, stop/resume and
solution paths. Check both orientations, e-ink refresh and largest heap block
before/after repeated entry/exit; allocation-failure handling needs validation.

Uses clock-seeded per-instance xorshift for game randomness only, not secrets.
Button-operated X3/X4 port; touch interaction is deferred. UI uses translation
keys with English fallback. No EPUB cache reset is required.

P1 validation is a C3 compile sanity check only (result in automated queue).
Host, simulator, soak, runtime-memory and hardware tests remain deferred to V1.
