#pragma once
#include <cstddef>
class MemoryStats {
 public:
  enum class Pool { Internal, Psram };
  struct Snapshot {
    bool available = false;
    size_t total = 0, free = 0, largest = 0, lowWater = 0;
  };
  // Independent allocator reads; this is not an atomic global memory snapshot.
  static Snapshot read(Pool pool);
};
