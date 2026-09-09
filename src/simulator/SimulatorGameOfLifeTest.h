#pragma once
#ifdef SIMULATOR
class GfxRenderer;
class MappedInputManager;
bool verifySimulatorGameOfLifeRules(GfxRenderer& renderer, MappedInputManager& input);
#endif
