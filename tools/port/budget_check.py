#!/usr/bin/env python3
"""
budget_check.py -- the memory gate for the X4 merge firmware.

The base X4 is an ESP32-C3 with ~380 KB usable SRAM and no PSRAM. Every
compiled-in activity contributes *resident* static data (globals, BSS,
`static` members, string tables) that is always present, plus a *transient*
cost (the heap-allocated activity object + its peak runtime heap) that exists
only while that one screen is on top of the stack.

This tool reads a manifest of per-app budgets and checks two things:

  1. RESIDENT: the sum of every app's static cost + the core resident cost
     (framebuffer, fonts, i18n table, system reserve) must leave room.
  2. TRANSIENT: for the single most expensive foreground app, resident +
     that app's (object + peak heap) must fit under the SRAM ceiling on the
     C3. Apps that can't fit are only allowed if they declare tier: psram
     (the X4 Pro build), and are reported as such -- never silently shipped
     on the C3.

Exit code is non-zero if any `c3` app blows the budget, so CI can gate on it.

Usage:
  python3 tools/port/budget_check.py                # uses tools/port/app_budgets.yaml
  python3 tools/port/budget_check.py --manifest X   # custom manifest
  python3 tools/port/budget_check.py --json         # machine-readable summary
"""
from __future__ import annotations
import argparse
import json
import os
import sys

# ---- Hardware model (ESP32-C3, base X4). Tune with real map-file numbers. ----
TOTAL_SRAM      = 380 * 1024   # usable internal SRAM, bytes
FRAMEBUFFER     = 800 * 480 // 8  # 48000 B, one 1-bit framebuffer, always resident
CORE_RESIDENT   = 34 * 1024    # fonts + i18n table + core singletons (measure & refine)
SYSTEM_RESERVE  = 96 * 1024    # FreeRTOS, Arduino/IDF core, WiFi/BLE stack headroom,
                               # + anti-fragmentation slack. Conservative on purpose.

def ceiling() -> int:
    """Bytes available for (resident apps + one foreground app)."""
    return TOTAL_SRAM - FRAMEBUFFER - CORE_RESIDENT - SYSTEM_RESERVE

def load_manifest(path: str) -> dict:
    try:
        import yaml  # PyYAML
    except ImportError:
        sys.exit("ERROR: PyYAML not installed. `pip install pyyaml` "
                 "(CI installs it automatically).")
    with open(path, "r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}
    if "apps" not in data or not isinstance(data["apps"], list):
        sys.exit(f"ERROR: {path} must contain a top-level `apps:` list.")
    return data

VALID_TIERS = ("c3", "psram")
SIZE_FIELDS = ("static_bytes", "object_bytes", "peak_heap_bytes")

def validate(apps: list) -> list[str]:
    """Return a list of problems. The gate must fail closed: an entry it cannot
    interpret is an error, never a silent PASS."""
    problems, seen = [], {}
    for i, app in enumerate(apps):
        if not isinstance(app, dict):
            problems.append(f"apps[{i}]: expected a mapping, got {type(app).__name__}")
            continue
        name = app.get("name")
        where = f"apps[{i}]" + (f" ({name})" if name else "")
        if not name or not isinstance(name, str):
            problems.append(f"{where}: missing or non-string `name`")
        elif name in seen:
            problems.append(f"{where}: duplicate name, already declared at apps[{seen[name]}]")
        else:
            seen[name] = i
        tier = app.get("tier", "c3")
        if tier not in VALID_TIERS:
            problems.append(
                f"{where}: tier {tier!r} is not one of {VALID_TIERS} "
                "(a typo here would otherwise let the app skip the gate)")
        for f in SIZE_FIELDS:
            if f not in app:
                problems.append(f"{where}: missing `{f}` (declare it, even if 0)")
                continue
            v = app[f]
            if isinstance(v, bool) or not isinstance(v, int):
                problems.append(f"{where}: `{f}` must be an integer, got {v!r}")
            elif v < 0:
                problems.append(f"{where}: `{f}` is negative ({v})")
    return problems

def app_cost(app: dict) -> tuple[int, int]:
    """Return (resident_static, transient) for one app, in bytes."""
    static = int(app.get("static_bytes", 0))
    obj    = int(app.get("object_bytes", 0))
    heap   = int(app.get("peak_heap_bytes", 0))
    return static, obj + heap

def human(n: int) -> str:
    return f"{n/1024:,.1f} KB"

def check(manifest: dict) -> tuple[bool, dict]:
    apps = manifest["apps"]
    cap = ceiling()

    resident_static = sum(app_cost(a)[0] for a in apps)
    core = FRAMEBUFFER + CORE_RESIDENT
    resident_total = resident_static + core

    rows, ok = [], True
    for a in apps:
        name = a.get("name", "?")
        tier = a.get("tier", "c3")
        static, transient = app_cost(a)
        # Worst case: this app is the foreground one on top of all resident data.
        footprint = resident_total + transient
        headroom = cap + core - footprint  # cap already excludes core; add back for compare
        fits = footprint <= (cap + core)
        if fits:
            status = "PASS"
        elif tier == "psram":
            status = "PSRAM-ONLY"   # over the C3 ceiling, but gated to the X4 Pro build
        else:
            # c3, or any tier validate() did not accept: fail closed.
            status, ok = "FAIL", False
        rows.append({
            "name": name, "tier": tier, "status": status,
            "static": static, "transient": transient,
            "footprint": footprint, "headroom": headroom,
        })

    summary = {
        "sram": TOTAL_SRAM, "ceiling": cap, "core_resident": core,
        "resident_static": resident_static, "resident_total": resident_total,
        "app_count": len(apps), "rows": rows, "ok": ok,
    }
    return ok, summary

def print_report(s: dict) -> None:
    bar = "-" * 72
    print(bar)
    print(f"  X4 MERGE -- MEMORY BUDGET GATE (ESP32-C3 / base X4)")
    print(bar)
    print(f"  Usable SRAM ......... {human(s['sram'])}")
    print(f"  Core resident ....... {human(s['core_resident'])}  (framebuffer + fonts + i18n)")
    print(f"  Resident app static . {human(s['resident_static'])}  ({s['app_count']} apps compiled in)")
    print(f"  -> resident total ... {human(s['resident_total'])}")
    print(f"  Foreground ceiling .. {human(s['ceiling'] + s['core_resident'])}")
    print(bar)
    print(f"  {'APP':<26}{'TIER':<7}{'STATIC':>9}{'PEAK':>10}{'HEADROOM':>11}  STATUS")
    print(bar)
    for r in sorted(s["rows"], key=lambda x: x["headroom"]):
        print(f"  {r['name']:<26}{r['tier']:<7}{human(r['static']):>9}"
              f"{human(r['transient']):>10}{human(r['headroom']):>11}  {r['status']}")
    print(bar)
    if s["ok"]:
        print("  RESULT: PASS -- every c3 app fits under the ceiling.\n")
    else:
        print("  RESULT: FAIL -- one or more c3 apps blow the budget.")
        print("          Shrink them, or move them to `tier: psram` (X4 Pro).\n")

def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description="X4 merge memory budget gate")
    ap.add_argument("--manifest", default=os.path.join(here, "app_budgets.yaml"))
    ap.add_argument("--json", action="store_true", help="emit JSON instead of a table")
    args = ap.parse_args()

    manifest = load_manifest(args.manifest)

    problems = validate(manifest["apps"])
    if problems:
        print("MANIFEST INVALID -- the gate cannot vouch for these entries:")
        for p in problems:
            print(f"  - {p}")
        print("\nFix the manifest. The gate fails closed on purpose: an entry it "
              "cannot interpret\nmust never be reported as PASS.")
        return 2

    ok, summary = check(manifest)
    if args.json:
        print(json.dumps(summary, indent=2))
    else:
        print_report(summary)
    return 0 if ok else 1

if __name__ == "__main__":
    raise SystemExit(main())
