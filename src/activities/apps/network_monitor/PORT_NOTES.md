# Network Monitor — partial source port, unverified

Adapts Biscuit's frame-observation and shared-SSID comparison modes. The menu
reuses PassiveMonitor Deauth for bounded frame parsing/views, and WifiScanner
for a single capped passive snapshot. Each child owns/releases its hold; the
menu has no radio ownership. Child allocation is fallible and failure logged.

Shared-SSID grouping adds 40 byte-sized indices to the existing scan activity.
Original SSID bytes are compared before display sanitization. Hidden SSIDs are
separate unknown identities. Per group, duplicate BSSIDs count once; displayed
open/protected values use the manager's boolean security metadata, not specific
cipher suites. Same SSID, channel differences and mixed protection are normal
in some networks and are never labeled rogue/attacker evidence. Capped snapshots
are not exhaustive surveys. Generic scan CSV export remains available.

The reference's source/BSSID event aggregation and rate-history graph are still
pending; existing bounded recent-event/burst views are available. All runtime,
RF, heap and SD checks remain deferred. No active probing, extra framebuffer,
background monitor, dynamic grouping collection or attack attribution added.

Device: Home → Tools → Defense → Network Monitor. Alternate both child views
and return/exit; verify radio release and next-app reuse. Scan same-name APs on
multiple channels, mixed security, repeated BSSIDs, hidden names and names that
look identical after control-byte sanitization. Check capped results, rescan,
view cycling, CSV, orientation, allocation failure and repeated-entry heap.
No cache reset required.
