"""Exercise the existing PlatformIO flash gate without building or flashing."""
import contextlib
import io
import os
from pathlib import Path
import runpy
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
with contextlib.redirect_stderr(io.StringIO()):
    GATE = runpy.run_path(str(ROOT / "scripts/check_firmware_size.py"))


class FakeEnv(dict):
    def GetProjectOption(self, name):
        return {
            "board_build.partitions": "partitions.csv",
            "custom_flash_reserve_bytes": "256K",
        }.get(name)

    def Exit(self, code):
        raise SystemExit(code)


class FirmwareSizeTest(unittest.TestCase):
    def test_fit_reserve_and_overflow_use_smallest_app_slot(self):
        cases = (
            ("fits", 700000, False, 0, "bytes free"),
            ("reserve warning", 900000, False, 0, "WARNING: FLASH HEADROOM LOW"),
            ("strict reserve", 900000, True, 1, "FLASH HEADROOM LOW"),
            ("oversized", 1048577, False, 1, "over by 1 bytes"),
            ("exact slot", 1048576, False, 0, "0 bytes free"),
        )
        with tempfile.TemporaryDirectory(prefix="crossink-flash-gate-") as directory:
            root = Path(directory)
            # Unequal OTA slots catch use of the larger slot; a non-app row
            # must not lower the image limit. Exercise hex and unit parsing.
            (root / "partitions.csv").write_text(
                "nvs,data,nvs,0x9000,0x5000\n"
                "a,app,ota_0,0x10000,1M\n"
                "b,app,ota_1,0x110000,2M\n"
            )
            firmware = root / "firmware.bin"
            env = FakeEnv(PROJECT_DIR=directory)
            for label, size, strict, expected, marker in cases:
                with self.subTest(label=label):
                    with firmware.open("wb") as stream:
                        stream.truncate(size)
                    output = io.StringIO()
                    code = 0
                    with patch.dict(os.environ, {"CROSSINK_FLASH_FAIL_UNDER_RESERVE": "1" if strict else "0"}):
                        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
                            try:
                                GATE["check_firmware_size"]([], [firmware], env)
                            except SystemExit as error:
                                code = error.code
                    self.assertEqual(code, expected, output.getvalue())
                    self.assertIn(marker, output.getvalue())


if __name__ == "__main__":
    unittest.main()
