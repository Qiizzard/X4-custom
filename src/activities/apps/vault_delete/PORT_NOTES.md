# Vault-file deletion — adapted Quick Wipe, wip/unverified

Biscuit's recursive wipe is deliberately narrowed to nine exact paths under
/crossink/vaults: passwords, totp and decoy, each with .bin/.next/.previous.
No recursion, generated paths, directory removal, overwrite loops, flash access,
credential/settings wipe or duress-triggered action. Recovery, books, caches,
Stego Notes images and copies/exports elsewhere remain untouched.

Confirm arms; a separate Page Forward press within 15 seconds deletes. Back or
timeout cancels; a simultaneous initial Confirm/Page Forward cannot delete.
The screen reports only successful remove calls, failures, and not-found paths.
The HAL's exists() cannot distinguish absence from read failure, so this limit
is shown. No guarantee of physical erasure on wear-levelled SD media.

Fixed scalar state and nine constant paths; no buffers beyond a 96-byte render
line, no file handles, recursion or app-owned dynamic allocation. The launcher
uses the existing fallible activity allocator. A missing SD produces failures.

Deferred V1/device: use disposable vault files on a test SD; cancel from review,
cancel while armed, timeout, simultaneous buttons, successful exact-path
removal and partial/full SD failure. Verify sentinel books, firmware/recovery,
settings and files outside the allowlist survive. Inspect SD independently;
not-found is not a verification of deletion. No cache reset required.
