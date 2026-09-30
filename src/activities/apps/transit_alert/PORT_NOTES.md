# Transit Alert — wip / unverified

Adapted from Biscuit TransitAlertActivity (MIT). Tools entry saves a named
fingerprint or monitors a saved entry. Left/Right selects; Confirm saves or
starts; Back stops monitoring/acknowledges a match, then exits. Match Confirm
resumes a fresh monitoring session. Visual only, no GPS/transit API/audio or
background reader integration. AP relocation/spoofing/missing scans can cause
false matches or missed alerts; this is never proof of arrival.

Owned WifiScan passive scans, first 20 HAL results ranked by RSSI, strongest
five distinct valid unicast/nonzero BSSIDs used for BOTH enrollment/comparison.
This intentionally fixes reference's asymmetry of five saved versus 20 scanned
(which makes 60% impossible in a busy snapshot). Jaccard intersection/union,
integer percent, threshold >=60; empty scan resets score to zero, error stops.
Nominal 15 seconds after previous scan completion; blocking scan can delay
buttons. Monitor prevents auto-sleep only for at most 30 minutes plus an
in-flight scan. Alert stops radio and permits sleep. No deep-sleep wake alert.

Fixed activity arrays: 32 x 63-byte stops, 20 x 42-byte scan results,
30-byte fingerprint, 32 slot indices = 2,918 bytes, plus scalars/base activity.
Fallible activity allocation keeps these off the stack and absent outside the
activity; no new framebuffer or per-scan app heap allocations. SDK scan memory,
keyboard and radio peaks unmeasured. Largest explicit local record is 71 bytes.

Persistence: /crossink/transit/stop-00.dat through stop-31.dat, exclusive create,
checked write/sync/close; no automatic overwrite/delete/import. Failed new-file
cleanup logged; existing malformed records retained and reported. A missing
card during directory/exists calls can still be indistinguishable from absence;
exclusive creation prevents replacing existing files. No secret storage claim.
TRN1 record, 71 bytes: magic[4], ASCII name[32] with final NUL, count[1] (1..5),
BSSID[5][6], FNV-1a checksum[4] little endian covering first 67 bytes. Checksum
is corruption detection, not authentication. Invalid records skipped, valid
records remain usable. To remove stops, manage those exact files on SD while
app is closed. No in-app destructive action and no legacy raw-struct import.

V1 / hardware still required: X4 Tools > Transit Alert; save 1/5 AP fingerprints,
reopen; exact/partial/no matches and <=20 capped scans; duplicate/zero/multicast
BSSIDs; moved/spoofed AP uncertainty; scan failures/other radio owner; corrupt,
truncated, full32 slots, unavailable/read-only/full SD, interrupted save; verify
no existing file replacement; 30-minute timeout, alert/resume/Back/Home/sleep,
input latency, both orientations, heap/stack recovery. No cache reset needed.
Compile-only result recorded in queue; no runtime/SD/RF tests claimed.
