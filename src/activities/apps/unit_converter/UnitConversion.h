#pragma once
// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
// Unit conversion tables and maths. Arduino-free and UI-free so the host tests
// (test/unit_conversion/) exercise exactly what the device runs -- worth doing
// here because a converter that is quietly wrong is worse than one that is
// obviously broken, and nothing on screen would tell you.
//
// The tables are flat `constexpr` arrays rather than biscuit's
// `static const std::vector<Category>` of nested vectors: that construct
// allocates on first use and lives in DRAM for the life of the process. These
// live in flash (RULESET rule 4) and cost no RAM at all.

#include <cstddef>
#include <cstdint>

namespace units {

struct Unit {
  const char* name;
  const char* abbreviation;
  // value_in_base = value * scale + offset
  double scale;
  double offset;
};

struct Category {
  const char* name;
  const Unit* units;
  size_t unitCount;
};

const Category* categories(size_t* outCount);

// Convert between two units of the same category, via that category's base
// unit. Returns false (leaving *out untouched) for an out-of-range index, so a
// caller cannot mistake a failure for a zero result.
bool convert(size_t categoryIndex, size_t fromUnit, size_t toUnit, double value, double* out);

// Format a converted value into `out` at a readable precision: enough
// significant figures to be useful across the range (bytes to terabytes)
// without printing 14 meaningless decimals. Always null-terminates.
void format(double value, char* out, size_t outCapacity);

}  // namespace units
