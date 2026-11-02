#!/usr/bin/env python3
"""Run production Sudoku, Chess, Tetris, Minesweeper, Maze, Snake and Blackjack logic plus Flashcards/Event Logger file checks in a disposable simulator filesystem."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MARKER = "GAME TEST RESULT: PASS sudoku seeds=32 empty=1 invalid=4 unsatisfiable=1"


REQUIRED_MARKERS = {
    "MatrixRain": "GAME TEST RESULT: PASS matrix_rain frames=6144",
    "Voronoi": "GAME TEST RESULT: PASS voronoi fixtures=64",
    "Sudoku": MARKER,
    "DiceRoller": "GAME TEST RESULT: PASS dice_roller fixtures=2688",
    "GameOfLife": "Game of Life block, wrapped blinker and restart counters and 1024 neighbourhood fixtures passed",
    **{name: f"GAME TEST RESULT: PASS {marker}" for name, marker in (
        ("Chess", "chess"), ("Blackjack", "blackjack"), ("Tetris", "tetris"),
        ("Minesweeper", "minesweeper"), ("Maze", "maze"), ("Snake", "snake"),
        ("Flashcards", "flashcards"), ("EventLogger", "event_logger"),
        ("Habits", "habits"), ("Transit", "transit"), ("CasinoSave", "casino_save"),
        ("CasinoGameplay", "casino_gameplay"), ("CasinoSettlement", "casino_settlement"),
    )},
}


def evaluate_result(returncode, text):
    missing = [name for name, marker in REQUIRED_MARKERS.items() if marker not in text]
    return returncode == 0 and not missing, missing


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
        decks = Path(directory) / "fs_" / "crossink" / "flashcards"
        decks.mkdir(parents=True)
        (Path(directory) / "fs_" / "crossink" / "logs").mkdir()
        (Path(directory) / "fs_" / "crossink" / "transit").mkdir()
        for count in (0, 511, 512, 513):
            scan_dir = decks.parent / f"scan{count}"
            scan_dir.mkdir()
            for index in range(count):
                (scan_dir / f"ignored{index}.txt").touch()
        (decks.parent / "scan_file").write_bytes(b"not a directory")
        fixtures = {
            "valid.csv": b"front,back\r\n\nquestion,answer,comma",
            "max.csv": ((b"f" * 127) + b"," + (b"b" * 127) + b"\n") * 32,
            "empty.csv": b"", "no_comma.csv": b"invalid", "no_front.csv": b",back",
            "no_back.csv": b"front,", "nul.csv": b"front,ba\x00ck",
            "long_front.csv": b"f" * 128 + b",back",
            "long_back.csv": b"front," + b"b" * 128,
            "too_many.csv": b"a,b\n" * 33, "oversize.csv": b"a" * 16385,
        }
        (decks / "directory.csv").mkdir()
        for ignored in ("notes.txt", "upper.CSV", ".csv", "x" * 60 + ".csv"):
            (decks / ignored).write_bytes(b"a,b")
        for name, payload in fixtures.items():
            (decks / name).write_bytes(payload)
        with args.log.open("w") as stream:
            try:
                result = subprocess.run([str(program)], cwd=directory, env=env, stdout=stream,
                                        stderr=subprocess.STDOUT, timeout=180)
            except subprocess.TimeoutExpired:
                print(f"FAIL: timeout; {args.log}")
                return 1
    text = args.log.read_text(errors="replace")
    ok, missing = evaluate_result(result.returncode, text)
    print(f"{'PASS' if ok else 'FAIL'}: {'/'.join(REQUIRED_MARKERS)} production logic, "
          f"exit {result.returncode}; {args.log}")
    if missing:
        print(f"Missing success markers: {', '.join(missing)}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
