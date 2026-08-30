#!/usr/bin/env python3
"""
new_app.py -- scaffold a new app that is correct-by-construction against the
ruleset. Emits an Activity skeleton (already wired to tr(), UITheme, the Back
button, and the render/loop lifecycle) and appends a budget stub.

Usage:
  python3 tools/port/new_app.py "Morse Code" --category tools
  python3 tools/port/new_app.py "Packet Capture" --category recon --tier psram

Then: fill in the TODOs, measure the budget numbers, add the STR_* i18n keys,
register it in the launcher, and run the port checklist.

Apps are emitted under src/activities/apps/<slug>/ because that is what the
firmware actually compiles (build_src_filter = +<*> covers src/, not the repo
root). An app parked outside src/ is a file nobody builds and nobody tests --
v1 shipped apps-ported/ as a staging area and the gate never ran on it.
"""
from __future__ import annotations
import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
TEMPLATE = os.path.join(HERE, "app_template")
APPS_DIR = os.path.join(ROOT, "src", "activities", "apps")
BUDGETS = os.path.join(HERE, "app_budgets.yaml")

CATEGORIES = {"reader", "recon", "tools", "comms", "defense", "games", "settings"}

def classname(name: str) -> str:
    parts = re.split(r"[^A-Za-z0-9]+", name)
    return "".join(p[:1].upper() + p[1:] for p in parts if p)

def slug(name: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")

def render_template(text: str, subs: dict) -> str:
    for k, v in subs.items():
        text = text.replace("{{" + k + "}}", v)
    return text

def main() -> int:
    ap = argparse.ArgumentParser(description="scaffold a ruleset-compliant app")
    ap.add_argument("name", help='human name, e.g. "Morse Code"')
    ap.add_argument("--category", required=True, choices=sorted(CATEGORIES))
    ap.add_argument("--tier", default="c3", choices=["c3", "psram"])
    ap.add_argument("--force", action="store_true", help="overwrite if it exists")
    args = ap.parse_args()

    cls = classname(args.name)
    if not cls:
        sys.exit("ERROR: could not derive a class name from that.")
    sl = slug(args.name)
    subs = {
        "NAME": args.name, "CLASS": cls, "SLUG": sl,
        "CATEGORY": args.category, "UPPER": sl.upper(),
    }

    out_dir = os.path.join(APPS_DIR, sl)
    h_out = os.path.join(out_dir, f"{cls}Activity.h")
    c_out = os.path.join(out_dir, f"{cls}Activity.cpp")
    if os.path.exists(out_dir) and not args.force:
        sys.exit(f"ERROR: {out_dir} already exists (use --force to overwrite).")
    os.makedirs(out_dir, exist_ok=True)

    for src, dst in ((os.path.join(TEMPLATE, "AppTemplate.h"), h_out),
                     (os.path.join(TEMPLATE, "AppTemplate.cpp"), c_out)):
        with open(src, "r", encoding="utf-8") as f:
            body = render_template(f.read(), subs)
        with open(dst, "w", encoding="utf-8") as f:
            f.write(body)

    # Append a budget stub (text append preserves the file's comments/order).
    stub = (f"\n  - name: {cls}\n"
            f"    tier: {args.tier}\n"
            f"    static_bytes: 0        # TODO measure\n"
            f"    object_bytes: 0        # TODO sizeof({cls}Activity)\n"
            f"    peak_heap_bytes: 0     # TODO measure peak\n")
    with open(BUDGETS, "a", encoding="utf-8") as f:
        f.write(stub)

    print(f"created  src/activities/apps/{sl}/{cls}Activity.h")
    print(f"created  src/activities/apps/{sl}/{cls}Activity.cpp")
    print(f"budget   appended a stub for '{cls}' to app_budgets.yaml")
    print()
    print("next:")
    print(f"  1. add STR_APP_{sl.upper()} + any STR_* to lib/I18n/translations/*.yaml")
    print(f"  2. fill in the render()/loop() TODOs")
    print(f"  3. measure and fill the 3 budget numbers")
    print(f"  4. register {cls}Activity in src/activities/apps/AppRegistry.cpp ({args.category})")
    print(f"  5. run: python3 tools/port/budget_check.py")
    print(f"  6. work docs/merge/PORT_CHECKLIST.md before opening the PR")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
