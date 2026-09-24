# Voronoi — source port, unverified

Adapted from Biscuit's nearest-point, dither and region-boundary rendering.
The cell cache is fixed at 6,000 bytes, with 40 point records (320 bytes).
Both live in the fallibly allocated activity, not on the task stack. A minimum
8-pixel cell grows as needed to cover either orientation within 100x60 cells.
Edge cells and point marks are clipped to the runtime safe area. There is no
second framebuffer or allocation during generation/rendering.

Confirm regenerates; Left/Right changes point count by five, from 5 to 40.
Cosmetic xorshift randomness is not cryptographic. Generation performs at most
240,000 nearest-point comparisons per explicit input; rendering uses cached
indices. No continuous animation or auto-sleep prevention. Hardware timing has
not been measured, so no responsiveness or power claim is made.

Device: Home → Tools → Games → Voronoi. Check both orientations, 5/40-point
limits, repeated regeneration, partial edge cells, dither/seed visibility,
input latency, auto-sleep and exit/re-entry heap. No cache reset required.
