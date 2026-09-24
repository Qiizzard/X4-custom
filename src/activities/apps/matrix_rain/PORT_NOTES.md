# Matrix Rain — source port, unverified

Adapted from Biscuit's `MatrixRainActivity`; keeps fixed column/drop state,
character grid and dithered trails. Cosmetic xorshift state replaces direct SDK
random calls; it is not cryptographic. The 2,736-byte grid and 624-byte column
arrays live in the fallibly allocated activity, not on the task stack. No second
framebuffer or per-frame allocation. Rendering uses the runtime safe area and
caps rows/columns to the fixed grid. Wide layouts may leave unused space.

Left/Right selects 3/2/1-second animation intervals. Up/Down adjusts density.
Confirm pauses/resumes. No busy-loop override or auto-sleep prevention. Headers,
status and hints use translations; the decorative alphabet is static in flash.

Deferred hardware checks: Home → Tools → Games → Matrix Rain; inspect portrait
and landscape trail clipping/ghosting, all controls, pause stability, normal
sleep timeout and exit/re-entry heap. Measure refresh duration and battery cost;
no performance or power improvement is claimed. No cache reset required.
