# Event Logger — wip / unverified

Adapted from Biscuit EventLoggerActivity (MIT, Copyright 2025 Dave Allie).
One fallible 6600-byte entry buffer is allocated on entry and released on exit;
it is too large for the task stack. A ring retains the latest 50 records without
vector growth or temporary strings. Rendering uses bounded UTF-8 line slices.
The shared keyboard is allocated only when composing and limits notes to 127
bytes. Its transient allocation estimate remains unverified on hardware.

Notes append only on explicit keyboard submission to `/crossink/logs/events.csv`.
No migration of `/biscuit/logs/eventlog.csv` occurs. Format: unsigned 32-bit
uptime milliseconds, comma, single-line UTF-8 note, newline. Commas in notes
are preserved. Uptime resets at reboot and wraps; it is not a date or clock.
The list is newest-first by append order, not sorted by uptime.

Read work is bounded to a 64 KiB file. Larger/malformed/torn records block
appends and show a storage error; no existing history is erased. Archive or
repair the file externally before reopening the app. Writes, sync and close
are checked, but append is not power-loss atomic: an interrupted save can
leave a torn record requiring repair. Errors do not claim successful saves.
No persistent write occurs on selection or viewing. Notes are plaintext.

Hardware: Tools → Event Logger, Right to compose, Confirm to view, Back to
return. Test cancel/empty notes, commas and multibyte text, reopen persistence,
more than 50 notes, missing/full SD, malformed/torn/oversized logs, reboot
uptime, and repeated exits while monitoring free heap/largest allocation.
No EPUB cache reset is required. Touch interaction remains deferred.

Only the P3 C3 compile sanity check is scheduled now; see the queue for its
result. Host, simulator, soak, filesystem-failure and hardware tests remain V1.
