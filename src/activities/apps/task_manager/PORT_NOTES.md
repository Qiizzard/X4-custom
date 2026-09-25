# Task Manager — diagnostics source port, unverified

Adapts Biscuit's heap/system views, with internal and PSRAM pools reported
separately. `lib/Memory/MemoryStats` isolates SDK allocator calls from the app;
API declarations were checked in the installed ESP-IDF headers. It allocates
nothing and returns unavailable in the simulator or for an absent pool.
Heap readings are independent, not atomic. Low-water metric sums per-heap
minima, which may have occurred at different times. No fragmentation percentage
or memory-performance improvement is claimed.

Confirm cycles internal RAM, PSRAM, and SD/display/radio status. Five-second
refresh, normal auto-sleep, bounded scalar snapshot and 48-byte owner buffer.
Radio status describes only RadioManager holds, not proof all RF is off.
Boot ticks wrap after approximately 49 days, labeled. CPU/flash details remain
in base Settings. There is no OS task list, task termination, restart, cache
clearing, radio acquisition/shutdown or storage write.

Device: Home → Tools → Task Manager. Compare internal metrics with serial
allocator readings, check unavailable PSRAM on C3 and separate PSRAM on S3,
SD readiness, display dimensions and radio hold state; exercise page cycling,
five-second refresh, both orientations, sleep and repeated entry/exit heap.
Simulator should report unavailable, never fabricated zeros. No cache reset.
