# Screen Decoy — source port, unverified

Adapts Biscuit's four cosmetic choices and preview/activation flow. It does not
call the reference's direct sleep path: normal framework sleep/recovery remains
in charge and may replace the view. No lock, encryption, radio shutdown, file
operation, PIN handling or privacy guarantee is implied. These limits appear
in selection before preview. Confirm activates only after preview; Back cancels.
Back or mapped Confirm returns from an active decoy to selection.

Only scalar state and a flash-resident four-entry translation-key table are
added; all views use the existing framebuffer and runtime safe area. Text is
translated, including a short decorative reading passage. No growing strings,
per-frame allocations or auto-sleep override.

Device: Home → Tools → Defense → Screen Decoy. Check all four previews,
activation/cancel, blank-screen exit, input and text in both orientations,
normal sleep/wake and recovery behavior. Verify no file/radio/security state
changes. No cache reset needed; no device tests performed yet.
