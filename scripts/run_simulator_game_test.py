#!/usr/bin/env python3
"""Run production Sudoku, Chess, Tetris, Minesweeper, Maze, Snake and Blackjack logic checks in a disposable simulator filesystem."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MARKER = "GAME TEST RESULT: PASS sudoku seeds=32 empty=1 invalid=4 unsatisfiable=1"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, required=True)
    args = parser.parse_args()
    program = ROOT / ".pio/build/simulator/program"
    if not program.is_file():
        parser.error("Build the simulator first")
    args.log.parent.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    for key in tuple(env):
        if key.startswith(("CROSSINK_SIMULATOR_SMOKE_", "CROSSINK_SIMULATOR_SOAK_", "CROSSINK_SIMULATOR_GAME_")):
            del env[key]
    env.update(SDL_VIDEODRIVER="dummy", CROSSINK_SIMULATOR_GAME_TEST="1")
    with tempfile.TemporaryDirectory(prefix="x4-game-tests-") as directory:
        (Path(directory) / "fs_").mkdir()
        with args.log.open("w") as stream:
            try:
                result = subprocess.run([str(program)], cwd=directory, env=env, stdout=stream,
                                        stderr=subprocess.STDOUT, timeout=180)
            except subprocess.TimeoutExpired:
                print(f"FAIL: timeout; {args.log}")
                return 1
    text = args.log.read_text(errors="replace")
    ok = result.returncode == 0 and MARKER in text and "GAME TEST RESULT: PASS chess" in text and "GAME TEST RESULT: PASS blackjack" in text and "GAME TEST RESULT: PASS tetris" in text and "GAME TEST RESULT: PASS minesweeper" in text and "GAME TEST RESULT: PASS maze" in text and "GAME TEST RESULT: PASS snake" in text
    print(f"{'PASS' if ok else 'FAIL'}: Sudoku/Chess/Tetris/Minesweeper/Maze/Snake/Blackjack production logic, exit {result.returncode}; {args.log}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
