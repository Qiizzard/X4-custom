// Derived from biscuit (https://github.com/yattsu/biscuit), MIT licence,
// Copyright (c) 2025 Dave Allie. Re-audited against docs/merge/RULESET.md;
// see PORT_NOTES.md in this directory for what changed and why.
#include "UnitConversion.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace units {
namespace {

// Each category's first unit is its base (scale 1, offset 0); everything else
// converts through it.
constexpr Unit kLength[] = {
    {"Meter", "m", 1.0, 0.0},         {"Kilometer", "km", 1000.0, 0.0}, {"Centimeter", "cm", 0.01, 0.0},
    {"Millimeter", "mm", 0.001, 0.0}, {"Mile", "mi", 1609.344, 0.0},    {"Yard", "yd", 0.9144, 0.0},
    {"Foot", "ft", 0.3048, 0.0},      {"Inch", "in", 0.0254, 0.0},
};

constexpr Unit kWeight[] = {
    {"Kilogram", "kg", 1.0, 0.0},         {"Gram", "g", 0.001, 0.0},
    {"Milligram", "mg", 0.000001, 0.0},   {"Pound", "lb", 0.45359237, 0.0},
    {"Ounce", "oz", 0.028349523125, 0.0}, {"Tonne", "t", 1000.0, 0.0},
};

// Temperature is the one category with a real offset, and the one biscuit got
// closest to wrong: F -> C is (F - 32) * 5/9, i.e. scale 5/9 and offset
// -32 * 5/9. Writing the exact fractions rather than 0.5556 / -17.7778 keeps
// round-trips (C -> F -> C) exact instead of drifting a hundredth of a degree.
constexpr Unit kTemperature[] = {
    {"Celsius", "C", 1.0, 0.0},
    {"Fahrenheit", "F", 5.0 / 9.0, -32.0 * 5.0 / 9.0},
    {"Kelvin", "K", 1.0, -273.15},
};

constexpr Unit kData[] = {
    {"Byte", "B", 1.0, 0.0},
    {"Kilobyte", "KB", 1024.0, 0.0},
    {"Megabyte", "MB", 1048576.0, 0.0},
    {"Gigabyte", "GB", 1073741824.0, 0.0},
    {"Terabyte", "TB", 1099511627776.0, 0.0},
    {"Bit", "bit", 0.125, 0.0},
};

constexpr Unit kSpeed[] = {
    {"Metres/sec", "m/s", 1.0, 0.0},
    {"Km/hour", "km/h", 1.0 / 3.6, 0.0},
    {"Miles/hour", "mph", 0.44704, 0.0},
    {"Knot", "kn", 1852.0 / 3600.0, 0.0},
};

constexpr Category kCategories[] = {
    {"Length", kLength, sizeof(kLength) / sizeof(kLength[0])},
    {"Weight", kWeight, sizeof(kWeight) / sizeof(kWeight[0])},
    {"Temperature", kTemperature, sizeof(kTemperature) / sizeof(kTemperature[0])},
    {"Data", kData, sizeof(kData) / sizeof(kData[0])},
    {"Speed", kSpeed, sizeof(kSpeed) / sizeof(kSpeed[0])},
};

constexpr size_t kCategoryCount = sizeof(kCategories) / sizeof(kCategories[0]);

}  // namespace

const Category* categories(size_t* outCount) {
  if (outCount != nullptr) *outCount = kCategoryCount;
  return kCategories;
}

bool convert(const size_t categoryIndex, const size_t fromUnit, const size_t toUnit, const double value, double* out) {
  if (out == nullptr || categoryIndex >= kCategoryCount) return false;
  const Category& category = kCategories[categoryIndex];
  if (fromUnit >= category.unitCount || toUnit >= category.unitCount) return false;

  const Unit& from = category.units[fromUnit];
  const Unit& to = category.units[toUnit];

  // Into the base unit, then back out of it. The offset is applied after the
  // scale going in, so it is removed before the divide coming out.
  const double base = value * from.scale + from.offset;
  if (to.scale == 0.0) return false;
  *out = (base - to.offset) / to.scale;
  return true;
}

namespace {

// Strip the trailing zeros a fixed-precision format leaves behind, so 2.5000
// reads as 2.5 and 1000.00 as 1000.
void trimTrailingZeros(char* text) {
  char* dot = std::strchr(text, '.');
  if (dot == nullptr) return;
  char* end = text + std::strlen(text) - 1;
  while (end > dot && *end == '0') {
    *end-- = '\0';
  }
  if (end == dot) *end = '\0';
}

// snprintf into a scratch buffer, reporting whether the whole rendering fit.
// Wrapping it means the "did it actually fit" question is answered once, at the
// one place it can be answered, instead of being reasoned about at each call.
bool renderFits(char* scratch, const size_t scratchCapacity, const char* format, const double value) {
  const int needed = snprintf(scratch, scratchCapacity, format, value);
  if (needed < 0 || static_cast<size_t>(needed) >= scratchCapacity) {
    scratch[0] = '\0';
    return false;
  }
  return true;
}

// Copy a finished rendering out, but only if the whole thing fits.
bool publishIfItFits(const char* scratch, char* out, const size_t outCapacity) {
  const size_t length = std::strlen(scratch);
  if (length == 0 || length >= outCapacity) return false;
  std::memcpy(out, scratch, length + 1);
  return true;
}

}  // namespace

void format(const double value, char* out, const size_t outCapacity) {
  if (out == nullptr || outCapacity == 0) return;
  out[0] = '\0';
  if (!std::isfinite(value)) {
    if (outCapacity > 1) {
      out[0] = '-';
      out[1] = '\0';
    }
    return;
  }

  // Render into scratch first, and only publish what fits whole.
  //
  // The rule this enforces: a truncated numeral is not a rounded one, it is a
  // *different* number. "1234567.89" cut to "1234567" reads as a plausible
  // answer, and "1.235e+06" cut to "1.235e+" is not even a number. Neither may
  // reach the caller, so nothing is copied to `out` until it is known to fit.
  char scratch[64];
  const double magnitude = std::fabs(value);

  // %g would render 1000000 as 1e+06, which is not what someone converting
  // gigabytes wants to read, so fixed notation is preferred in the normal range.
  const bool preferCompact = magnitude != 0.0 && (magnitude >= 1e9 || magnitude < 1e-4);
  if (!preferCompact) {
    const char* fixedFormat = magnitude >= 1000.0 ? "%.2f" : magnitude >= 1.0 ? "%.4f" : "%.6f";
    if (renderFits(scratch, sizeof(scratch), fixedFormat, value)) {
      trimTrailingZeros(scratch);
      if (publishIfItFits(scratch, out, outCapacity)) return;
    }
  }

  // Fixed notation did not fit (or was never the right choice). Try compact
  // forms, shortest last, and take the first that fits whole.
  //
  // The ladder stops at two significant figures on purpose. One sig fig would
  // render 1234567.89 as "1e+06" -- 19% low, and indistinguishable on screen
  // from a real result. For a converter, a value that wrong is worse than an
  // honest "it does not fit", so anything narrower gets the marker below.
  static constexpr const char* kCompactFormats[] = {"%.4g", "%.3g", "%.2g"};
  for (const char* fmt : kCompactFormats) {
    if (!renderFits(scratch, sizeof(scratch), fmt, value)) continue;
    if (publishIfItFits(scratch, out, outCapacity)) return;
  }

  // Nothing fits. Say so rather than show a fragment of a number.
  if (outCapacity > 1) {
    out[0] = '*';
    out[1] = '\0';
  }
}

}  // namespace units
