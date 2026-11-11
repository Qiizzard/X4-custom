#!/usr/bin/env python3
"""Run production Sudoku, Chess, Tetris, Minesweeper, Maze, Snake and Blackjack logic plus Flashcards/Event Logger file checks in a disposable simulator filesystem."""
import argparse
import csv
import io
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MARKER = "GAME TEST RESULT: PASS sudoku seeds=32 empty=1 invalid=4 unsatisfiable=1"


REQUIRED_MARKERS = {
    "MdnsCsv": "GAME TEST RESULT: PASS mdns_csv rows=8 exports=2 failures=2 recovery=1",
    "HostCsv": "GAME TEST RESULT: PASS host_csv rows=32 exports=2 exhausted=1 recovery=1",
    "VaultDelete": "GAME TEST RESULT: PASS vault_delete removed=9 absent=9 failed=1",
    "EtchExport": "GAME TEST RESULT: PASS etch_export count=2 recovery=1",
    "VisualOrientations": "GAME TEST RESULT: PASS visual_orientations count=4",
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


def validate_etch_bmp(data):
    """Validate the exact three-pixel export fixture; return dimensions or None."""
    if len(data) < 62:
        return None
    width, height = struct.unpack_from("<II", data, 18)
    if (width, height) not in ((160, 240), (240, 160)):
        return None
    stride = ((width + 31) // 32) * 4
    expected = bytearray(stride * height)
    for x, y in ((0, 0), (7, 1), (width - 1, height - 1)):
        expected[(height - 1 - y) * stride + x // 8] |= 128 >> (x % 8)
    valid = (data[:2] == b"BM" and len(data) == 62 + len(expected)
             and struct.unpack_from("<I", data, 2)[0] == len(data)
             and struct.unpack_from("<I", data, 10)[0] == 62
             and struct.unpack_from("<I", data, 14)[0] == 40
             and struct.unpack_from("<HHI", data, 26) == (1, 1, 0)
             and struct.unpack_from("<I", data, 34)[0] == len(expected)
             and data[54:62] == b"\xff\xff\xff\0\0\0\0\0"
             and data[62:] == expected)
    return (width, height) if valid else None


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
        mdns = decks.parent / "mdns"
        mdns.mkdir()
        for slot in (0, *range(3, 100)):
            (mdns / f"services-{slot:02d}.csv").write_bytes(b"existing mDNS export")
        (mdns.parent / "mdns-blocker").write_bytes(b"preserve obstruction")
        hosts = decks.parent / "hosts"
        hosts.mkdir()
        (hosts.parent / "hosts-blocker").write_bytes(b"preserve host obstruction")
        for slot in (0, *range(3, 100)):
            (hosts / f"scan-{slot:02d}.csv").write_bytes(b"existing export")
        vaults = decks.parent / "vaults"
        vaults.mkdir()
        removed_names = [f"{name}.{suffix}" for name in ("passwords", "totp", "decoy")
                         for suffix in ("bin", "next", "previous")]
        for name in removed_names:
            (vaults / name).write_bytes(b"disposable fixture")
        preserved_names = ("passwords.bin.backup", "other.bin", "nested/passwords.bin")
        (vaults / "nested").mkdir()
        for name in preserved_names:
            (vaults / name).write_bytes(b"preserve exact bytes")

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
        if any((vaults / name).exists() for name in removed_names):
            print("FAIL: listed vault fixture survived deletion")
            return 1
        if any(not (vaults / name).is_file() or (vaults / name).read_bytes() != b"preserve exact bytes"
               for name in preserved_names):
            print("FAIL: unrelated vault fixture changed")
            return 1
        names = ["'=SUM(1,2)", "' +formula", "'-formula", "'@formula", "plain,comma", "", '"' * 64, "ordinary"]
        services = ["_http", "_https", "_printer", "_ipp", "_googlecast", "_sonos", "_homeassistant", "_mqtt"]
        rows = [["instance", "hostname", "ipv4", "ipv6", "port", "service"]]
        rows += [[name, f"host-{i}.local", f"192.0.2.{i+1}", f"fe80::{i+1}%3",
                  str(65535 if i == 7 else 80+i), services[i] + "._tcp"] for i, name in enumerate(names)]
        for slot in (1, 2):
            path = mdns / f"services-{slot:02d}.csv"
            if not path.is_file() or list(csv.reader(io.StringIO(path.read_text()), strict=True)) != rows:
                print("FAIL: mDNS CSV escaping, formula protection or record mismatch")
                return 1
        if (any((mdns / f"services-{slot:02d}.csv").read_bytes() != b"existing mDNS export"
                for slot in (0, *range(3, 100))) or len(list(mdns.iterdir())) != 100
                or (mdns.parent / "mdns-blocker").read_bytes() != b"preserve obstruction") :
            print("FAIL: mDNS CSV changed existing file or unexpected outputs")
            return 1
        ports = b"21;22;23;25;53;80;139;443;445;993;3389;5900;8080;8443"
        expected = b"ipv4,open_tcp_ports,tested_tcp_ports\n"
        for index in range(32):
            expected += (f'192.0.2.{index + 1},"'.encode()
                         + (ports if index % 2 else b"") + b'","' + ports + b'"\n')
        for slot in range(100):
            path = hosts / f"scan-{slot:02d}.csv"
            if not path.is_file() or path.read_bytes() != (expected if slot in (1, 2) else b"existing export"):
                print(f"FAIL: Host CSV content or preservation mismatch at slot {slot}")
                return 1
        if (len(list(hosts.iterdir())) != 100
                or (hosts.parent / "hosts-blocker").read_bytes() != b"preserve host obstruction"):
            print("FAIL: unexpected Host CSV output")
            return 1
        drawings = list((decks.parent / "drawings").glob("*.bmp"))
        if len(drawings) != 2:
            print("FAIL: expected two distinct exported BMP files")
            return 1
        dimensions = set()
        for drawing in drawings:
            data = drawing.read_bytes()
            size = validate_etch_bmp(data)
            if size is None:
                print("FAIL: BMP header/palette/pixel/padding mismatch")
                return 1
            dimensions.add(size)
        if len(dimensions) != 2:
            print("FAIL: missing BMP orientation")
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
