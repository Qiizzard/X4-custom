#!/usr/bin/env python3
"""Self-test for the budget gate: proves it PASSES good apps, FAILS an
oversized c3 app, ALLOWS an oversized psram app, and -- crucially -- REFUSES
manifests it cannot interpret instead of passing them. Run in CI."""
import importlib.util
import os

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("bc", os.path.join(HERE, "budget_check.py"))
bc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bc)

HOG = 10_000_000


def _one(name, tier, heap):
    return {"apps": [{"name": name, "tier": tier, "static_bytes": 256,
                      "object_bytes": 512, "peak_heap_bytes": heap}]}


# ---- the original three: the happy paths -----------------------------------

def test_small_c3_app_passes():
    ok, s = bc.check(_one("Tiny", "c3", 0))
    assert ok and s["rows"][0]["status"] == "PASS"


def test_oversized_c3_app_fails():
    ok, s = bc.check(_one("Hog", "c3", HOG))
    assert (not ok) and s["rows"][0]["status"] == "FAIL"


def test_oversized_psram_app_allowed():
    ok, s = bc.check(_one("Hog", "psram", HOG))
    assert ok and s["rows"][0]["status"] == "PSRAM-ONLY"


# ---- the gate must fail closed ---------------------------------------------
# Before this, any tier string that was not exactly "c3" or "psram" fell through
# to PASS, so `tier: C3` silently disabled the gate for that app.

def test_malformed_tier_is_rejected_by_validation():
    for bad in ["C3", "c3 ", " c3", "PSRAM", "typo", "", None, 3, True, ["c3"]]:
        problems = bc.validate(_one("Hog", bad, HOG)["apps"])
        assert problems, f"tier {bad!r} was accepted by validate()"
        assert any("tier" in p for p in problems), f"tier {bad!r}: {problems}"


def test_malformed_tier_never_reports_pass():
    # Even if validation were bypassed, an over-budget app must not read PASS.
    for bad in ["C3", "typo", None]:
        ok, s = bc.check(_one("Hog", bad, HOG))
        assert s["rows"][0]["status"] == "FAIL", f"tier {bad!r} -> {s['rows'][0]['status']}"
        assert not ok


def test_missing_size_fields_are_rejected():
    problems = bc.validate([{"name": "NoSizes", "tier": "c3"}])
    assert len([p for p in problems if "missing" in p]) == 3, problems


def test_negative_sizes_are_rejected():
    # A negative static_bytes used to be summed into the shared resident total,
    # raising headroom for every other app in the manifest.
    problems = bc.validate([{"name": "Neg", "tier": "c3", "static_bytes": -10_000_000,
                             "object_bytes": 0, "peak_heap_bytes": 0}])
    assert any("negative" in p for p in problems), problems


def test_duplicate_names_are_rejected():
    # new_app.py --force appends a second budget stub; duplicates were silently
    # summed into resident_static.
    e = {"name": "Dup", "tier": "c3", "static_bytes": 1, "object_bytes": 1, "peak_heap_bytes": 1}
    problems = bc.validate([dict(e), dict(e)])
    assert any("duplicate" in p for p in problems), problems


def test_missing_name_is_rejected():
    problems = bc.validate([{"tier": "c3", "static_bytes": 0,
                             "object_bytes": 0, "peak_heap_bytes": 0}])
    assert any("name" in p for p in problems), problems


def test_the_shipped_manifest_is_valid():
    m = bc.load_manifest(os.path.join(HERE, "app_budgets.yaml"))
    problems = bc.validate(m["apps"])
    assert not problems, "shipped app_budgets.yaml is invalid:\n  " + "\n  ".join(problems)


if __name__ == "__main__":
    tests = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    for t in tests:
        t()
        print(f"  ok  {t.__name__}")
    print(f"OK: all {len(tests)} budget-gate self-tests passed")
