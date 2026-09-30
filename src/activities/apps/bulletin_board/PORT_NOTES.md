# Bulletin Board — wip/unverified

Adapts Biscuit's AP/browser board to a password-required, named RadioManager
AP hold. Confirm requests 8–63 printable-character WiFi password; two associated
clients max, channel 1, SSID X4-Board. Timer 5/15/30 minutes; Back/expiry/sleep
closes sockets before releasing the radio. Input password buffer is wiped;
SDK credential lifetime/NVS behavior remains a hardware validation item.
Latest 16 posts in RAM only; shared board with unverified authors, no accounts,
TLS, Internet gateway, SD persistence, editing or deletion. Stop discards posts.

HalBulletinServer is a separate HAL module because simulator replaces lib/hal.
Simulator start explicitly fails; no native radio/service is pretended. Device
server binds only the owned AP address. One nonblocking socket client, backlog
one, five-second total request/response deadline, at most 512 socket bytes per
poll. Fixed arrays (shared with Drop mode): 5121 request, 4096 response, 192 response header, 32 host,
16x201 posts = 12,657 bytes plus scalar/base/socket SDK memory (peak unmeasured).
No app-owned heap allocations except the fallible activity/secret-input child.

HTTP/1.1 only: GET /, GET /messages and POST /post. Headers <=1024 bytes;
post body 1–200 printable ASCII bytes. Reject duplicate Content-Length/Host,
transfer encoding, Expect, extra buffered/pipelined bytes, missing/wrong numeric
Host and missing/wrong X-X4-Board header on posts. No CORS support, so cross-origin
JS cannot satisfy that custom header; exact numeric Host also blocks DNS
rebinding hostnames. This is CSRF mitigation, not participant authentication.
Messages rendered with textContent, never inserted as markup. Translation labels
are HTML-escaped; JavaScript has no user-input interpolation. Generated portal
headers untouched; this is a small standalone response assembled in fixed buffers.

Deferred V1/device: connect two phones using configured password; open displayed
numeric URL; post up to 200 ASCII bytes, newest-16 rotation, browser text such
as <script> remains inert; wrong Host, oversized/duplicate lengths, chunked/
slow/partial/disconnected requests and cross-origin submissions. Confirm timer,
Back and sleep stop AP; then open another network app. Measure AP/socket peak
heap/largest block and SDK credential retention; no EPUB cache reset required.

## Small-file Dead Drop mode (2026-09-30)

Separate Comms entry uses the same timed AP UI with owner dead_drop and SSID
X4-Drop. GET /files lists exact slots; POST /upload stores raw binary (including
empty) up to 4096 bytes; GET /file-NN.bin downloads only slots 00–31. Other
methods/paths, encoded traversal and out-of-range indices rejected. Custom
header/Host/length/deadline limits remain; board endpoints are unavailable in
Drop mode and vice versa. Each complete upload opens an exclusive new file
under /crossink/drop, checks write/sync/close, and best-effort removes failed
partial output. No file is opened until the bounded request body is complete.
Lost HTTP responses may leave a completed upload; inspect before retrying.
Downloads check size/type and close before sending; byte content is unchanged.

Original filenames are not preserved. Complete files persist after session
stop; no overwrite or deletion endpoint. 32 slots x 4096 bytes = 128 KiB max
created content. Files added externally can exceed this; oversize downloads
are rejected. Full/unreadable SD or occupied slots return a request failure;
failed cleanup can leave a partial numbered file. Shared participants may read
all slot files; no per-file auth/TLS/identity claim. Browser links use textContent
and download attributes, served bytes use application/octet-stream + nosniff.
The request buffer grows by 3840 bytes to hold headers + one 4 KiB upload;
no streaming allocation, upload vector or secondary framebuffer. Fixed server
arrays total 12,657 bytes for either mode; SDK/heap peak remains unmeasured.

Embedded browser JavaScript syntax checked with Node; no runtime/HTTP/SD tests
claimed. Deferred V1/device: empty/binary-NUL/4096-byte round trips, 4097 rejection,
all 32 slots/full condition, traversal/encoded paths, wrong Host/custom header,
partial/slow/disconnected uploads, full/removed SD, cleanup failure, timer/Back/
sleep and another network app afterward. Verify unrelated files survive, source
names map to numbered files, and completed files persist. Use disposable data;
no cache reset required. Larger-file streaming is not implemented.
