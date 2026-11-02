"""Checks that incomplete or failed simulator runs cannot report success."""
import struct
import unittest
from run_simulator_game_test import REQUIRED_MARKERS, evaluate_result, validate_etch_bmp


class RunnerResultTests(unittest.TestCase):
    def test_complete_success(self):
        self.assertEqual(evaluate_result(0, "\n".join(REQUIRED_MARKERS.values())), (True, []))

    def test_each_missing_suite_fails(self):
        for omitted in REQUIRED_MARKERS:
            with self.subTest(suite=omitted):
                text = "\n".join(value for key, value in REQUIRED_MARKERS.items() if key != omitted)
                self.assertEqual(evaluate_result(0, text), (False, [omitted]))

    def test_nonzero_exit_fails_even_with_markers(self):
        for code in (1, 2, -9):
            self.assertEqual(evaluate_result(code, "\n".join(REQUIRED_MARKERS.values())), (False, []))

    def test_empty_output_fails(self):
        self.assertEqual(evaluate_result(0, ""), (False, list(REQUIRED_MARKERS)))


class BmpValidatorTests(unittest.TestCase):
    @staticmethod
    def fixture(landscape=False):
        # Explicit sizes/offsets for the three pixels, independent of validator loops.
        width, height, payload, offsets = ((240, 160, 5120, (5088, 5056, 29)) if landscape
                                            else (160, 240, 4800, (4780, 4760, 19)))
        data = bytearray(62 + payload)
        data[:2] = b"BM"
        for offset, value in ((2, len(data)), (10, 62), (14, 40), (18, width), (22, height), (34, payload)):
            struct.pack_into("<I", data, offset, value)
        struct.pack_into("<HH", data, 26, 1, 1)
        data[54:57] = b"\xff\xff\xff"
        for offset, value in zip(offsets, (128, 1, 1)):
            data[62 + offset] = value
        return data

    def test_valid_orientations(self):
        self.assertEqual(validate_etch_bmp(self.fixture()), (160, 240))
        self.assertEqual(validate_etch_bmp(self.fixture(True)), (240, 160))

    def test_header_and_pixel_mutations(self):
        for offset in (0, 2, 10, 14, 18, 22, 26, 28, 30, 34, 54, 58, 62, 62 + 4780):
            with self.subTest(offset=offset):
                data = self.fixture()
                data[offset] ^= 1
                self.assertIsNone(validate_etch_bmp(data))

    def test_padding_and_length(self):
        data = self.fixture(True)
        data[62 + 30] = 1  # Landscape row has two padding bytes.
        self.assertIsNone(validate_etch_bmp(data))
        for data in (b"", self.fixture()[:61], self.fixture()[:-1], self.fixture() + b"x"):
            self.assertIsNone(validate_etch_bmp(data))


if __name__ == "__main__":
    unittest.main()
