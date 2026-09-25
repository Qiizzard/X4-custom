#include "MemoryStats.h"
#ifndef SIMULATOR
#include <esp_heap_caps.h>
#endif
MemoryStats::Snapshot MemoryStats::read(Pool pool) {
  Snapshot result;
#ifndef SIMULATOR
  const uint32_t caps = MALLOC_CAP_8BIT | (pool == Pool::Internal ? MALLOC_CAP_INTERNAL : MALLOC_CAP_SPIRAM);
  result.total = heap_caps_get_total_size(caps);
  result.available = result.total != 0;
  result.free = heap_caps_get_free_size(caps);
  result.largest = heap_caps_get_largest_free_block(caps);
  result.lowWater = heap_caps_get_minimum_free_size(caps);
#else
  (void)pool;
#endif
  return result;
}
