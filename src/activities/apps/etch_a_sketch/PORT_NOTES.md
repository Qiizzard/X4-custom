# Etch-A-Sketch — wip / unverified

Adapted from `biscuit-reference/src/activities/apps/EtchASketchActivity.{h,cpp}`,
MIT, Copyright (c) 2025 Dave Allie.

One fallible 4,800-byte allocation in onEnter, released in onExit, holds a
160x240 drawing (240x160 in landscape). This is too large for the task stack
and is needed only during the activity. Rendering scales logical pixels into
the bezel-safe content area. No second full-resolution framebuffer is used.
The exported BMP preserves the logical resolution, not the display resolution.

Directions draw; Page Forward lifts/lowers the pen; Confirm saves; Back exits.
Unsaved drawing state is not persisted on exit. Idle auto-sleep is allowed.
BMP exports use `/crossink/drawings`, exclusive file creation, a 32-byte local
row buffer and checked writes, sync and explicit close. Save failure is shown
in the UI; incomplete files created by that save are removed where possible.
No per-frame or per-row allocations. Translation keys use English fallback.

Validation for this batch is C3 compile sanity only (result in the queue).
Host/simulator/soak and physical storage/display/heap checks are deferred to V1.
Hardware: Tools → Etch-A-Sketch; draw in both orientations, lift/reposition the
pen, save twice, and open the BMPs on a computer. Verify pixel orientation,
black/white palette, absence of cursor in exports, and earlier-file retention.
Try missing/full/read-only SD media and repeated entry/exit while watching free
heap and largest allocatable block. No EPUB cache reset is required. This is
currently a button-operated port; touch drawing is not implemented.
