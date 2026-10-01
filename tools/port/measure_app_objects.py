#!/usr/bin/env python3
"""Extract compiled activity object sizes from a firmware ELF's DWARF records.

These are sizeof values, NOT measured peak heap, stack or static RAM budgets.
Requires pyelftools (available in the PlatformIO Python environment).
"""
import argparse
import hashlib
import json
from pathlib import Path
from elftools.elf.elffile import ELFFile


def measure(path):
    sizes = {}
    with path.open("rb") as stream:
        elf = ELFFile(stream)
        if not elf.has_dwarf_info():
            raise ValueError("ELF has no DWARF information")
        dwarf = elf.get_dwarf_info()
        for unit in dwarf.iter_CUs():
            name = unit.get_top_DIE().attributes.get("DW_AT_name")
            if not name or b"/activities/apps/" not in name.value.replace(b"\\", b"/"):
                continue
            for die in unit.iter_DIEs():
                if die.tag not in ("DW_TAG_class_type", "DW_TAG_structure_type"):
                    continue
                name = die.attributes.get("DW_AT_name")
                size = die.attributes.get("DW_AT_byte_size")
                if not name or not size or not name.value.endswith(b"Activity"):
                    continue
                key = name.value.decode("utf-8")
                if key in sizes and sizes[key] != size.value:
                    raise ValueError(f"Conflicting compiled sizes for {key}")
                sizes[key] = size.value
    if not sizes:
        raise ValueError("No complete app activity types found; no size evidence produced")
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    return {"elf_sha256": digest,
            "scope": "Compiled object bytes only; excludes transient allocations, stack and static RAM.",
            "object_bytes": dict(sorted(sizes.items()))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = json.dumps(measure(args.elf), indent=2) + "\n"
    if args.output:
        args.output.write_text(result)
    else:
        print(result, end="")


if __name__ == "__main__":
    main()
