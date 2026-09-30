#pragma once
#include <HalClock.h>
#include <Logging.h>

#include <cstdint>

// The native simulator's clock does not implement boot-verified NTP sync.
// Keep this capability unavailable there; never substitute host wall time.
namespace HalClockSync {
inline bool syncSystemTimeFromNTP() {
#ifdef SIMULATOR
  LOG_ERR("CLOCK", "Verified NTP synchronization unavailable in simulator");
  return false;
#else
  return halClock.syncSystemTimeFromNTP();
#endif
}
inline bool getSyncedUnixTime(uint64_t& seconds) {
#ifdef SIMULATOR
  seconds = 0;
  return false;
#else
  return halClock.getSyncedUnixTime(seconds);
#endif
}
}  // namespace HalClockSync
