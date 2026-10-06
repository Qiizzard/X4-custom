"""Runner contract tests; these do not execute or pass a firmware soak."""
import argparse
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import subprocess

ROOT = Path(__file__).resolve().parents[2]

def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'scripts' / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

class IsolationTest(unittest.TestCase):
    def test_soak_contract(self):
        runner = load('run_simulator_soak_test')
        inherited = {'CROSSINK_SIMULATOR_GAME_TEST': '1', 'CROSSINK_SIMULATOR_SMOKE_TEST': '1',
                     'CROSSINK_SIMULATOR_SOAK_IDLE_MS': '1', 'KEEP_ME': 'yes'}
        def fake_run(command, **kw):
            env = kw['env']
            self.assertNotIn('CROSSINK_SIMULATOR_GAME_TEST', env)
            self.assertNotIn('CROSSINK_SIMULATOR_SMOKE_TEST', env)
            self.assertEqual(env['CROSSINK_SIMULATOR_SOAK_IDLE_MS'], '600000')
            self.assertEqual(env['CROSSINK_SIMULATOR_SOAK_CYCLES'], '50')
            self.assertEqual(env['KEEP_ME'], 'yes')
            self.assertEqual(kw['timeout'], 850)
            # A successful process WITHOUT a soak marker must still fail.
            return subprocess.CompletedProcess(command, 0)
        with tempfile.TemporaryDirectory() as directory, patch.dict(os.environ, inherited), patch.object(runner.subprocess, 'run', side_effect=fake_run):
            self.assertFalse(runner.run_one('snake', Path('/unused'), Path(directory)))

    def test_smoke_contract(self):
        runner = load('run_simulator_smoke_test')
        inherited = {'CROSSINK_SIMULATOR_GAME_TEST': '1', 'CROSSINK_SIMULATOR_SOAK_TEST': '1',
                     'CROSSINK_SIMULATOR_SMOKE_THEME': 'stale', 'KEEP_ME': 'yes'}
        def fake_run(command, **kw):
            env = kw['env']
            self.assertNotIn('CROSSINK_SIMULATOR_GAME_TEST', env)
            self.assertNotIn('CROSSINK_SIMULATOR_SOAK_TEST', env)
            self.assertNotIn('CROSSINK_SIMULATOR_SMOKE_THEME', env)
            self.assertEqual(env['CROSSINK_SIMULATOR_SMOKE_PAGE_TURNS'], '3')
            self.assertEqual(env['KEEP_ME'], 'yes')
            return subprocess.CompletedProcess(command, 2, 'intentional test failure\n')
        with tempfile.TemporaryDirectory() as directory:
            book = Path(directory) / 'book.epub'
            book.touch()
            args = argparse.Namespace(book=str(book), build=False, env='simulator', page_turns=3,
                                      theme=None, headless=True, timeout=120)
            with patch.dict(os.environ, inherited), patch.object(runner, 'program_path', return_value=book), patch.object(runner.subprocess, 'run', side_effect=fake_run):
                self.assertEqual(runner.run_smoke(args), 2)

if __name__ == '__main__':
    unittest.main()
