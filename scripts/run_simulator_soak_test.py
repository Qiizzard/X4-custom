#!/usr/bin/env python3
"""Run 50-cycle / 10-minute app lifecycle checks in disposable simulator filesystems.

Build with `pio run -e simulator` first. This covers entry screens, not every
interactive app path. Logs and exit codes are retained in --output-dir.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import os
from pathlib import Path
import subprocess
import tempfile

APPS = ("qr_generator", "cipher", "otp_generator", "clock", "game_of_life")
ROOT = Path(__file__).resolve().parents[1]


def run_one(app: str, program: Path, output: Path) -> bool:
    with tempfile.TemporaryDirectory(prefix=f"crossink-soak-{app}-") as directory:
        (Path(directory) / "fs_").mkdir()
        env = os.environ.copy()
        # Inherited debug settings must not shorten the gate or run two harnesses.
        for key in tuple(env):
            if key.startswith(("CROSSINK_SIMULATOR_SMOKE_", "CROSSINK_SIMULATOR_SOAK_")):
                del env[key]
        env.update(SDL_VIDEODRIVER="dummy", CROSSINK_SIMULATOR_SOAK_TEST="1",
                   CROSSINK_SIMULATOR_SOAK_APP=app, CROSSINK_SIMULATOR_SOAK_CYCLES="50",
                   CROSSINK_SIMULATOR_SOAK_IDLE_MS="600000")
        log = output / f"{app}.log"
        with log.open("w") as stream:
            try:
                result = subprocess.run([str(program)], cwd=directory, env=env,
                                        stdout=stream, stderr=subprocess.STDOUT, timeout=850)
            except subprocess.TimeoutExpired:
                print(f"{app}: FAIL (850-second timeout), {log}", flush=True)
                return False
        text = log.read_text(errors="replace")
        ok = result.returncode == 0 and f"SOAK RESULT: PASS app={app} cycles=50" in text
        print(f"{app}: {'PASS' if ok else 'FAIL'} (exit {result.returncode}), {log}", flush=True)
        return ok


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apps", nargs="+", choices=APPS, default=list(APPS))
    parser.add_argument("--jobs", type=int, default=1, help="Independent simulator processes (1-5)")
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.jobs <= len(APPS):
        parser.error("--jobs must be between 1 and 5")
    if len(set(args.apps)) != len(args.apps):
        parser.error("--apps must not contain duplicates")
    program = ROOT / ".pio/build/simulator/program"
    if not program.is_file():
        parser.error("Build the simulator first: pio run -e simulator")
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(lambda app: run_one(app, program, output), args.apps))
    return 0 if all(results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
