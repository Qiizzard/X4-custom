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
poll. Fixed arrays: 1281 request, 4096 response, 192 response header, 32 host,
16x201 posts = 8,817 bytes plus scalar/base/socket SDK memory (peak unmeasured).
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
