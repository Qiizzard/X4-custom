# Nearby chat — Mesh Chat source port, wip/unverified

Explicit Confirm starts a named EspNow hold on channel 1. RadioManager owns
init, broadcast peer, callback registration, sending and shutdown. SDK APIs
verified against installed ESP-IDF esp_now.h and existing NearbyStatsSync.
A short critical section detaches the sink and waits out its bounded queue
copy before activity destruction. Callback only copies; loop parses up to four
frames per iteration. Simulator returns unavailable, not fake received data.

Biscuit 224-byte CHAT frame: type 1, claimed MAC at 1..6, name at 7..22,
text at 23..222, hop at 223. Outgoing fixed identity CrossInk and local station
MAC, up to 64 typed characters, hop 0. Incoming exact-size type-1 frames retain
up to 199 text bytes; display sanitizes non-ASCII/control bytes. Claimed own-MAC
frames ignored; sender identity is not authenticated. No encryption, reliable
delivery or acknowledgments. Peer/relay behavior is described below.
Local successful send means SDK queue acceptance only, explicitly shown.

Eight 217-byte message records, a 1024-byte SPSC queue and a reused 224-byte
packet buffer (~3 KiB fixed data plus scalar/base overhead). Existing keyboard
child is allocated fallibly; no per-frame vectors/strings. Receive display
updates at most once per second; four 50-character pages per message. Queue
drops shown. Back/exit releases radio; normal auto-sleep remains enabled.

Deferred V1/device: two disposable test devices, Comms -> Nearby chat ->
Confirm on both; send ASCII text, inspect messages/parts, compare Biscuit
224-byte compatibility, flood queue/malformed lengths, busy acquisition,
failed send, keyboard cancel/OOM, normal sleep and repeated enter/exit. Check
RADIO hold reports and another network app after exit; no cache reset needed.
RF/heap/callback lifetime behavior is not yet validated on hardware. AP-based comms follow.

## Peers and opt-in relay

Presence uses the reference 23-byte type-2 frame every nominal 10 seconds
while the parent loop runs. Up to 16 claimed sender MAC/name entries expire
90 seconds after processing their last presence/chat. Relayed identities are
not evidence of direct radio proximity. Page Back cycles messages/peers/relay;
Left/Right browses peers. The relay screen requires Confirm to enable forwarding;
a simultaneous view-change/Confirm does not toggle it. Default off each entry.

Forwarded type-1 frames increment hop count only if below 3; frames above 3
are rejected. A separate 1 KiB queue bounds pending forwards and drops overflow;
one relay send per second at most, no automatic retry. Disable clears its queue.
A 16-entry FNV-1a ring over all 223 non-hop bytes expires after 30 seconds;
it suppresses duplicates/own-send echoes best effort, not cryptographically.
Hash collisions and repeated identical legitimate messages may be suppressed;
ring churn may allow duplicate display, but TTL/rate/queue bounds still hold.
Queue-drop counter resets when relay toggles. No message/presence persistence.
Adds 448-byte peer array (C3 layout), 128 bytes hash/times and 1 KiB relay queue
plus scalar state. No growing peer vectors or callback parsing/allocation.

Deferred V1/device: two/three-device presence and expiry, name sanitization,
more than 16 peers, duplicate/TTL boundaries, disabled relay, explicit toggle,
simultaneous controls, queue overflow, one-per-second forwarding, all-WiFi
lifecycle transitions and callback teardown under receive load. Observe actual
on-air behavior before claiming compatibility or reliable multi-hop coverage.
