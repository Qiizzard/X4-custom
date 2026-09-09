#pragma once
// SimulatorSoakTest -- drives the PORT_CHECKLIST.md soak gate for one app:
// open/close N times (heap must return to baseline) then leave it running
// idle for a configurable duration (heap must stay flat, no crash/hang).
//
// Opt-in via CROSSINK_SIMULATOR_SOAK_TEST so it never runs during a normal
// simulator session or the regular smoke-test walk.

#ifdef SIMULATOR
void runSimulatorSoakTestTick();
#endif
