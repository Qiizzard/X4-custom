# Sudoku — wip / unverified

Adapted from `biscuit-reference/src/activities/apps/SudokuActivity.{h,cpp}`,
MIT, Copyright (c) 2025 Dave Allie.

Board, clue flags and iterative solver state live in the activity (324 array bytes).
Generation permutes a valid completed grid and removes 51 cells using a bounded
shuffle; no recursive task-stack growth. Puzzle uniqueness is not guaranteed,
matching the original removal approach. The solver validates givens and attempts
at most 256 cells per loop, so Back can cancel. Manual valid completion is detected.

Hardware: Games → Sudoku. Move with directions, tap Confirm to cycle 1–9,
Page Forward to clear, hold Confirm on an editable cell to solve. Check clue
protection, inconsistent givens, manual completion, solver cancellation and new
puzzles. Check both orientations and heap after repeated entry/exit.

Uses clock-seeded per-instance xorshift for game randomness only, not secrets.
Button-operated X3/X4 port; touch interaction is deferred. UI uses translation
keys with English fallback. No EPUB cache reset is required.

P1 validation is a C3 compile sanity check only (result in automated queue).
Host, simulator, soak, runtime-memory and hardware tests remain deferred to V1.
