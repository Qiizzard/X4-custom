#ifdef SIMULATOR

#include "SimulatorSoakTest.h"

#include <Arduino.h>
#include <I18n.h>
#include <Logging.h>

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>

#if defined(__linux__)
#include <malloc.h>
#define CROSSINK_SOAK_HAVE_MALLINFO2 1
#elif defined(__APPLE__)
#include <malloc/malloc.h>
#define CROSSINK_SOAK_HAVE_MALLOC_ZONE 1
#endif

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "activities/apps/AppLauncherActivity.h"
#include "activities/apps/AppRegistry.h"

extern ActivityManager activityManager;
extern GfxRenderer renderer;
extern MappedInputManager mappedInputManager;

namespace {

// The only apps this harness knows how to name from an env var. Scoped to
// exactly what docs/merge/PORT_LEDGER.md lists as needing the soak gate --
// add a row here if a future session soaks a different app.
struct SoakAppLookup {
  const char* envName;
  StrId title;
};

constexpr SoakAppLookup kSoakApps[] = {
    {"qr_generator", StrId::STR_APP_QR_GENERATOR},   {"cipher", StrId::STR_APP_CIPHER},
    {"otp_generator", StrId::STR_APP_OTP_GENERATOR}, {"clock", StrId::STR_APP_CLOCK},
    {"game_of_life", StrId::STR_APP_GAME_OF_LIFE},
};

enum class SoakStep : uint8_t {
  Start,
  WarmupHome,
  WarmupSettle,
  OpenApp,
  OpenSettle,
  CloseApp,
  CloseSettle,
  MeasureAfterLoop,
  IdleOpenSettle,
  IdleWait,
  Done,
};

// ESP.getFreeHeap() in the simulator is a hardcoded constant (see
// Arduino.h's simulator stub) -- it never moves, so it cannot detect a leak
// here. What actually reflects this process's live allocations is glibc's
// malloc arena, via mallinfo2(). This is a simulator-only diagnostic; on
// real hardware ESP.getFreeHeap() (used by the ClockActivity/OtpGenerator
// logging elsewhere) is the correct, meaningful signal.
uint64_t processHeapUsedBytes() {
#ifdef CROSSINK_SOAK_HAVE_MALLINFO2
  const struct mallinfo2 mi = mallinfo2();
  return static_cast<uint64_t>(mi.uordblks) + static_cast<uint64_t>(mi.hblkhd);
#elif defined(CROSSINK_SOAK_HAVE_MALLOC_ZONE)
  malloc_statistics_t stats{};
  malloc_zone_statistics(nullptr, &stats);
  return static_cast<uint64_t>(stats.size_in_use);
#else
  return 0;
#endif
}

class SimulatorSoakTest {
 public:
  void tick() {
    if (!enabled()) return;
    try {
      tickImpl();
    } catch (const std::exception& e) {
      fail("Unhandled exception: %s", e.what());
    } catch (...) {
      fail("Unhandled non-standard exception");
    }
  }

 private:
  SoakStep step = SoakStep::Start;
  int settleFrames = 0;
  int cyclesDone = 0;
  int cyclesTotal = 50;
  unsigned long idleDurationMs = 600000UL;  // 10 minutes, per PORT_CHECKLIST.md
  unsigned long idleStartMs = 0;
  unsigned long idleLastLogMs = 0;
  uint64_t baselineUsed = 0;
  uint64_t afterLoopUsed = 0;
  uint64_t idleBaselineUsed = 0;
  // Fixed slack for allocator fragmentation/bookkeeping noise between runs,
  // This is a bounded regression check: growth smaller than this threshold
  // can escape detection, so a PASS is not proof of zero leaks.
  static constexpr uint64_t kSlackBytes = 4096;
  std::unique_ptr<Activity> (*appFactory)(GfxRenderer&, MappedInputManager&) = nullptr;
  const char* appEnvName = nullptr;

  static bool enabled() { return std::getenv("CROSSINK_SIMULATOR_SOAK_TEST") != nullptr; }

  [[noreturn]] static void fail(const char* message) {
    LOG_ERR("SOAK", "%s", message);
    std::_Exit(2);
  }

  template <typename... Args>
  [[noreturn]] static void fail(const char* format, Args... args) {
    logPrintf("ERR", "SOAK", format, args...);
    logPrintf("ERR", "SOAK", "\n");
    std::_Exit(2);
  }

  static int envInt(const char* name, int fallback) {
    const char* raw = std::getenv(name);
    if (raw == nullptr || raw[0] == '\0') return fallback;
    errno = 0;
    char* end = nullptr;
    const long value = std::strtol(raw, &end, 10);
    if (errno == ERANGE || end == raw || *end != '\0' || value < 0 || value > INT_MAX) {
      fail("Invalid non-negative integer for %s", name);
    }
    return static_cast<int>(value);
  }

  std::unique_ptr<Activity> (*resolveAppFactory())(GfxRenderer&, MappedInputManager&) {
    const char* requested = std::getenv("CROSSINK_SIMULATOR_SOAK_APP");
    if (requested == nullptr || requested[0] == '\0') {
      fail(
          "CROSSINK_SIMULATOR_SOAK_APP is required (e.g. qr_generator, cipher, otp_generator, clock, "
          "game_of_life)");
    }
    for (const auto& lookup : kSoakApps) {
      if (std::strcmp(lookup.envName, requested) != 0) continue;
      size_t categoryCount = 0;
      const AppCategoryInfo* categories = appCategories(&categoryCount);
      for (size_t c = 0; c < categoryCount; ++c) {
        size_t appCount = 0;
        const AppEntry* apps = appsInCategory(categories[c].category, &appCount);
        for (size_t a = 0; a < appCount; ++a) {
          if (apps[a].title == lookup.title) {
            appEnvName = lookup.envName;
            return apps[a].create;
          }
        }
      }
      fail("App '%s' is not registered in AppRegistry", requested);
    }
    fail("Unknown CROSSINK_SIMULATOR_SOAK_APP value: %s", requested);
  }

  void queueStep(SoakStep nextStep, int framesToSettle) {
    settleFrames = framesToSettle;
    step = nextStep;
  }

  void tickImpl() {
    mappedInputManager.simulatorClearInputFrame();

    if (settleFrames > 0) {
      --settleFrames;
      return;
    }

    switch (step) {
      case SoakStep::Start: {
        appFactory = resolveAppFactory();
        // A ten-minute inactivity timeout otherwise sleeps before the ten-minute
        // hold finishes (the 50 cycles consume additional time). Test process
        // only: do not persist this setting or alter the app's sleep policy.
        SETTINGS.sleepTimeoutMinutes = CrossPointSettings::SLEEP_TIMEOUT_NEVER_MINUTES;
        cyclesTotal = envInt("CROSSINK_SIMULATOR_SOAK_CYCLES", 50);
        idleDurationMs = static_cast<unsigned long>(envInt("CROSSINK_SIMULATOR_SOAK_IDLE_MS", 600000));
#if !defined(CROSSINK_SOAK_HAVE_MALLINFO2) && !defined(CROSSINK_SOAK_HAVE_MALLOC_ZONE)
        fail("This build needs glibc mallinfo2() or macOS malloc_zone_statistics() for real heap measurements");
#endif
        if (cyclesTotal < 50 || idleDurationMs < 600000UL || idleDurationMs > 86400000UL) {
          fail("Gate requires at least 50 cycles and 600000 ms idle (maximum 24 hours)");
        }
        LOG_INF("SOAK", "Starting soak test: app=%s cycles=%d idleMs=%lu", appEnvName, cyclesTotal, idleDurationMs);
        activityManager.goHome();
        queueStep(SoakStep::WarmupHome, 5);
        break;
      }

      case SoakStep::WarmupHome:
        // Let Home's own allocations (caches, file listings) settle before we
        // sample the baseline -- otherwise first-touch costs there would look
        // like a leak in the app we're actually testing.
        queueStep(SoakStep::WarmupSettle, 10);
        break;

      case SoakStep::WarmupSettle:
        baselineUsed = processHeapUsedBytes();
        LOG_INF("SOAK", "Baseline process heap in use: %llu bytes", static_cast<unsigned long long>(baselineUsed));
        queueStep(SoakStep::OpenApp, 1);
        break;

      case SoakStep::OpenApp: {
        auto activity = appFactory(renderer, mappedInputManager);
        if (!activity) fail("App factory returned nullptr on cycle %d", cyclesDone);
        activityManager.replaceActivity(std::move(activity));
        queueStep(SoakStep::OpenSettle, 3);
        break;
      }

      case SoakStep::OpenSettle:
        if (activityManager.requestUpdateAndWait() != RequestUpdateResult::Rendered) {
          fail("Render was rejected opening cycle %d", cyclesDone);
        }
        queueStep(SoakStep::CloseApp, 1);
        break;

      case SoakStep::CloseApp:
        activityManager.goHome();
        queueStep(SoakStep::CloseSettle, 3);
        break;

      case SoakStep::CloseSettle:
        if (activityManager.requestUpdateAndWait() != RequestUpdateResult::Rendered) {
          fail("Render was rejected closing cycle %d", cyclesDone);
        }
        ++cyclesDone;
        if (cyclesDone % 10 == 0 || cyclesDone == cyclesTotal) {
          LOG_INF("SOAK", "Cycle %d/%d done, process heap in use=%llu", cyclesDone, cyclesTotal,
                  static_cast<unsigned long long>(processHeapUsedBytes()));
        }
        if (cyclesDone < cyclesTotal) {
          queueStep(SoakStep::OpenApp, 1);
        } else {
          queueStep(SoakStep::MeasureAfterLoop, 5);
        }
        break;

      case SoakStep::MeasureAfterLoop: {
        afterLoopUsed = processHeapUsedBytes();
        const int64_t delta = static_cast<int64_t>(afterLoopUsed) - static_cast<int64_t>(baselineUsed);
        LOG_INF("SOAK", "After %d open/close cycles: process heap in use=%llu (baseline=%llu, delta=%lld)", cyclesTotal,
                static_cast<unsigned long long>(afterLoopUsed), static_cast<unsigned long long>(baselineUsed),
                static_cast<long long>(delta));
        if (afterLoopUsed > baselineUsed + kSlackBytes) {
          fail("Heap did not return to baseline after %d cycles: baseline=%llu after=%llu (grew %lld bytes)",
               cyclesTotal, static_cast<unsigned long long>(baselineUsed),
               static_cast<unsigned long long>(afterLoopUsed), static_cast<long long>(delta));
        }
        auto activity = appFactory(renderer, mappedInputManager);
        if (!activity) fail("App factory returned nullptr entering idle phase");
        activityManager.replaceActivity(std::move(activity));
        queueStep(SoakStep::IdleOpenSettle, 3);
        break;
      }

      case SoakStep::IdleOpenSettle:
        if (activityManager.requestUpdateAndWait() != RequestUpdateResult::Rendered) {
          fail("Render was rejected entering idle phase");
        }
        idleBaselineUsed = processHeapUsedBytes();
        idleStartMs = millis();
        idleLastLogMs = idleStartMs;
        LOG_INF("SOAK", "Entering %lu ms idle-running phase in %s", idleDurationMs, appEnvName);
        queueStep(SoakStep::IdleWait, 1);
        break;

      case SoakStep::IdleWait: {
        const unsigned long now = millis();
        if (now - idleLastLogMs >= 30000UL) {
          idleLastLogMs = now;
          LOG_INF("SOAK", "Idle %.1fs elapsed, process heap in use=%llu", (now - idleStartMs) / 1000.0,
                  static_cast<unsigned long long>(processHeapUsedBytes()));
        }
        if (now - idleStartMs >= idleDurationMs) {
          const uint64_t idleUsed = processHeapUsedBytes();
          const int64_t delta = static_cast<int64_t>(idleUsed) - static_cast<int64_t>(idleBaselineUsed);
          LOG_INF("SOAK", "Idle phase complete: process heap in use=%llu (baseline=%llu, delta=%lld)",
                  static_cast<unsigned long long>(idleUsed), static_cast<unsigned long long>(idleBaselineUsed),
                  static_cast<long long>(delta));
          if (idleUsed > idleBaselineUsed + kSlackBytes) {
            fail("Heap grew during idle run: baseline=%llu after_idle=%llu (grew %lld bytes)",
                 static_cast<unsigned long long>(idleBaselineUsed), static_cast<unsigned long long>(idleUsed),
                 static_cast<long long>(delta));
          }
          activityManager.goHome();
          queueStep(SoakStep::Done, 3);
        } else {
          // Keep driving one render every ~5s so a periodic-timer app (e.g.
          // Clock) actually exercises its running-in-background path rather
          // than sitting fully idle.
          if ((now - idleStartMs) % 5000UL < 20UL) {
            activityManager.requestUpdate();
          }
          queueStep(SoakStep::IdleWait, 1);
        }
        break;
      }

      case SoakStep::Done:
        if (processHeapUsedBytes() > baselineUsed + kSlackBytes) {
          fail("Heap did not return to Home baseline after idle app exit");
        }
        LOG_INF("SOAK", "SOAK RESULT: PASS app=%s cycles=%d baseline=%llu after_loop=%llu", appEnvName, cyclesTotal,
                static_cast<unsigned long long>(baselineUsed), static_cast<unsigned long long>(afterLoopUsed));
        std::_Exit(0);
    }
  }
};

SimulatorSoakTest soakTest;

}  // namespace

void runSimulatorSoakTestTick() { soakTest.tick(); }

#endif  // SIMULATOR
