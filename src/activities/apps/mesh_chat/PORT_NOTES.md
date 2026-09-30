# Nearby chat — Mesh Chat direct mode, wip/unverified

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
delivery, acknowledgments, relay, deduplication or presence/peer discovery yet.
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
RF/heap/callback lifetime behavior is not yet validated on hardware. Peer and
relay implementation remains next; AP-based comms follow.
