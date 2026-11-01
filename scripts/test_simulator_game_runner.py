"""Checks that incomplete or failed simulator runs cannot report success."""
import unittest
from run_simulator_game_test import REQUIRED_MARKERS, evaluate_result


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


if __name__ == "__main__":
    unittest.main()
