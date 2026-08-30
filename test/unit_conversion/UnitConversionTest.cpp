// Unit conversion tables and maths.
//
// Worth testing carefully: a converter is the kind of app people trust without
// checking, and a wrong scale factor looks exactly like a right one on screen.
// Temperature is the one with an offset, so it gets the most attention here --
// it is also where the original biscuit table used rounded constants
// (0.5556 / -17.7778) that do not round-trip cleanly.
#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <string>

#include "UnitConversion.h"

namespace {

// Category indices, matching the table order in UnitConversion.cpp.
constexpr size_t kLength = 0;
constexpr size_t kWeight = 1;
constexpr size_t kTemperature = 2;
constexpr size_t kData = 3;
constexpr size_t kSpeed = 4;

double convert(const size_t category, const size_t from, const size_t to, const double value) {
  double out = 0.0;
  EXPECT_TRUE(units::convert(category, from, to, value, &out));
  return out;
}

// Find a unit's index by abbreviation so the tests read in real units rather
// than magic numbers, and stay correct if a row is inserted.
size_t unitIndex(const size_t category, const char* abbreviation) {
  size_t count = 0;
  const units::Category* cats = units::categories(&count);
  EXPECT_LT(category, count);
  for (size_t i = 0; i < cats[category].unitCount; ++i) {
    if (std::strcmp(cats[category].units[i].abbreviation, abbreviation) == 0) return i;
  }
  ADD_FAILURE() << "no unit '" << abbreviation << "' in category " << category;
  return 0;
}

std::string formatted(const double value) {
  char buffer[32] = {};
  units::format(value, buffer, sizeof(buffer));
  return buffer;
}

TEST(UnitConversion, TableIsWellFormed) {
  size_t count = 0;
  const units::Category* cats = units::categories(&count);
  ASSERT_EQ(count, 5u);
  for (size_t c = 0; c < count; ++c) {
    EXPECT_NE(cats[c].name, nullptr);
    EXPECT_GT(cats[c].unitCount, 1u) << "a category with one unit converts nothing";
    for (size_t u = 0; u < cats[c].unitCount; ++u) {
      EXPECT_NE(cats[c].units[u].name, nullptr);
      EXPECT_NE(cats[c].units[u].abbreviation, nullptr);
      EXPECT_NE(cats[c].units[u].scale, 0.0) << "a zero scale would divide by zero on the way back";
    }
    // The first unit of every category is its base.
    EXPECT_DOUBLE_EQ(cats[c].units[0].scale, 1.0);
    EXPECT_DOUBLE_EQ(cats[c].units[0].offset, 0.0);
  }
}

TEST(UnitConversion, ConvertsLength) {
  EXPECT_NEAR(convert(kLength, unitIndex(kLength, "km"), unitIndex(kLength, "m"), 1.0), 1000.0, 1e-9);
  EXPECT_NEAR(convert(kLength, unitIndex(kLength, "mi"), unitIndex(kLength, "km"), 1.0), 1.609344, 1e-9);
  EXPECT_NEAR(convert(kLength, unitIndex(kLength, "in"), unitIndex(kLength, "cm"), 1.0), 2.54, 1e-9);
  EXPECT_NEAR(convert(kLength, unitIndex(kLength, "ft"), unitIndex(kLength, "in"), 1.0), 12.0, 1e-9);
  EXPECT_NEAR(convert(kLength, unitIndex(kLength, "yd"), unitIndex(kLength, "ft"), 1.0), 3.0, 1e-9);
}

TEST(UnitConversion, ConvertsWeight) {
  EXPECT_NEAR(convert(kWeight, unitIndex(kWeight, "kg"), unitIndex(kWeight, "g"), 1.0), 1000.0, 1e-9);
  EXPECT_NEAR(convert(kWeight, unitIndex(kWeight, "lb"), unitIndex(kWeight, "oz"), 1.0), 16.0, 1e-9);
  EXPECT_NEAR(convert(kWeight, unitIndex(kWeight, "kg"), unitIndex(kWeight, "lb"), 1.0), 2.20462262, 1e-6);
}

// The freezing/boiling anchors everyone knows. If any of these drift, the table
// is wrong in a way a user will notice immediately.
TEST(UnitConversion, ConvertsTemperatureAtTheAnchorPoints) {
  const size_t c = unitIndex(kTemperature, "C");
  const size_t f = unitIndex(kTemperature, "F");
  const size_t k = unitIndex(kTemperature, "K");

  EXPECT_NEAR(convert(kTemperature, f, c, 32.0), 0.0, 1e-9);
  EXPECT_NEAR(convert(kTemperature, f, c, 212.0), 100.0, 1e-9);
  EXPECT_NEAR(convert(kTemperature, c, f, 0.0), 32.0, 1e-9);
  EXPECT_NEAR(convert(kTemperature, c, f, 100.0), 212.0, 1e-9);
  EXPECT_NEAR(convert(kTemperature, c, k, 0.0), 273.15, 1e-9);
  EXPECT_NEAR(convert(kTemperature, k, c, 273.15), 0.0, 1e-9);
  // -40 is the crossing point, and a good check that the offset has the right sign.
  EXPECT_NEAR(convert(kTemperature, c, f, -40.0), -40.0, 1e-9);
  // Absolute zero.
  EXPECT_NEAR(convert(kTemperature, k, c, 0.0), -273.15, 1e-9);
  EXPECT_NEAR(convert(kTemperature, k, f, 0.0), -459.67, 1e-9);
}

// Using exact fractions rather than rounded decimals is what makes this hold.
TEST(UnitConversion, TemperatureRoundTripsWithoutDrift) {
  const size_t c = unitIndex(kTemperature, "C");
  const size_t f = unitIndex(kTemperature, "F");
  for (double celsius = -100.0; celsius <= 100.0; celsius += 6.25) {
    const double back = convert(kTemperature, f, c, convert(kTemperature, c, f, celsius));
    EXPECT_NEAR(back, celsius, 1e-9) << "C -> F -> C drifted at " << celsius;
  }
}

TEST(UnitConversion, ConvertsDataUsingBinaryMultiples) {
  EXPECT_NEAR(convert(kData, unitIndex(kData, "KB"), unitIndex(kData, "B"), 1.0), 1024.0, 1e-9);
  EXPECT_NEAR(convert(kData, unitIndex(kData, "MB"), unitIndex(kData, "KB"), 1.0), 1024.0, 1e-9);
  EXPECT_NEAR(convert(kData, unitIndex(kData, "GB"), unitIndex(kData, "MB"), 1.0), 1024.0, 1e-9);
  EXPECT_NEAR(convert(kData, unitIndex(kData, "TB"), unitIndex(kData, "GB"), 1.0), 1024.0, 1e-9);
  EXPECT_NEAR(convert(kData, unitIndex(kData, "B"), unitIndex(kData, "bit"), 1.0), 8.0, 1e-9);
}

TEST(UnitConversion, ConvertsSpeed) {
  EXPECT_NEAR(convert(kSpeed, unitIndex(kSpeed, "km/h"), unitIndex(kSpeed, "m/s"), 3.6), 1.0, 1e-9);
  EXPECT_NEAR(convert(kSpeed, unitIndex(kSpeed, "mph"), unitIndex(kSpeed, "km/h"), 1.0), 1.609344, 1e-9);
  EXPECT_NEAR(convert(kSpeed, unitIndex(kSpeed, "kn"), unitIndex(kSpeed, "km/h"), 1.0), 1.852, 1e-9);
}

// Every non-offset conversion must be reversible; this catches a mistyped
// scale factor anywhere in the tables without hand-writing a case per unit.
TEST(UnitConversion, EveryConversionRoundTrips) {
  size_t count = 0;
  const units::Category* cats = units::categories(&count);
  for (size_t c = 0; c < count; ++c) {
    for (size_t from = 0; from < cats[c].unitCount; ++from) {
      for (size_t to = 0; to < cats[c].unitCount; ++to) {
        const double forward = convert(c, from, to, 12.5);
        const double back = convert(c, to, from, forward);
        EXPECT_NEAR(back, 12.5, 1e-6) << cats[c].name << ": " << cats[c].units[from].abbreviation << " -> "
                                      << cats[c].units[to].abbreviation << " -> back";
      }
    }
  }
}

TEST(UnitConversion, ConvertingAUnitToItselfIsTheIdentity) {
  size_t count = 0;
  const units::Category* cats = units::categories(&count);
  for (size_t c = 0; c < count; ++c) {
    for (size_t u = 0; u < cats[c].unitCount; ++u) {
      EXPECT_NEAR(convert(c, u, u, 42.0), 42.0, 1e-9) << cats[c].units[u].abbreviation;
    }
  }
}

TEST(UnitConversion, RejectsOutOfRangeIndicesRatherThanReturningZero) {
  double out = 123.0;
  EXPECT_FALSE(units::convert(99, 0, 0, 1.0, &out));
  EXPECT_FALSE(units::convert(kLength, 99, 0, 1.0, &out));
  EXPECT_FALSE(units::convert(kLength, 0, 99, 1.0, &out));
  EXPECT_FALSE(units::convert(kLength, 0, 0, 1.0, nullptr));
  EXPECT_DOUBLE_EQ(out, 123.0) << "a rejected conversion must not overwrite the caller's value";
}

TEST(UnitConversion, FormatsReadably) {
  EXPECT_EQ(formatted(1.0), "1");
  EXPECT_EQ(formatted(2.5), "2.5");
  EXPECT_EQ(formatted(1000.0), "1000");
  EXPECT_EQ(formatted(0.0), "0");
  EXPECT_EQ(formatted(-40.0), "-40");
  // No trailing zero noise on a clean value.
  EXPECT_EQ(formatted(1024.0), "1024");
}

TEST(UnitConversion, FormatsVerySmallAndVeryLargeValuesWithoutLosingThem) {
  // A byte in terabytes: must not render as "0".
  const double tiny = convert(kData, unitIndex(kData, "B"), unitIndex(kData, "TB"), 1.0);
  const std::string tinyText = formatted(tiny);
  EXPECT_NE(tinyText, "0") << "a small-but-nonzero result was formatted away to zero";
  EXPECT_FALSE(tinyText.empty());

  const double huge = convert(kData, unitIndex(kData, "TB"), unitIndex(kData, "B"), 5.0);
  EXPECT_FALSE(formatted(huge).empty());
}

// A truncated number is not a rounded number, it is a different number.
// "1234567.89" cut to "1234567" would read as a plausible result.
TEST(UnitConversion, NeverEmitsATruncatedNumber) {
  // Deliberately too small for the fixed-notation form of this value.
  char tight[8] = {};
  units::format(1234567.89, tight, sizeof(tight));
  EXPECT_LT(std::strlen(tight), sizeof(tight));
  EXPECT_NE(std::string(tight), "1234567") << "a truncated value was presented as a result";
  // Whatever it fell back to must still parse as recognisably the same number.
  // Precision may legitimately drop to make it fit (1.2e+06); magnitude may not.
  const double parsed = std::atof(tight);
  EXPECT_GT(parsed, 0.0) << "fallback did not produce a number at all: " << tight;
  EXPECT_NEAR(parsed / 1234567.89, 1.0, 0.10) << "fallback lost the magnitude: " << tight;
}

// When even the shortest compact form cannot fit, say so rather than emit a
// fragment. A visible marker is honest; "1.235e+" is not.
TEST(UnitConversion, EmitsAMarkerRatherThanASingleSignificantFigure) {
  // "1e+06" would fit in 6 bytes but is 19% low. The ladder stops at two
  // significant figures and takes the marker instead.
  char narrow[7] = {};
  units::format(1234567.89, narrow, sizeof(narrow));
  EXPECT_EQ(std::string(narrow), "*");
}

TEST(UnitConversion, EmitsAMarkerWhenNothingFits) {
  char tiny[3] = {};
  units::format(1234567.89, tiny, sizeof(tiny));
  EXPECT_LT(std::strlen(tiny), sizeof(tiny));
  EXPECT_EQ(std::string(tiny), "*");
}

// Degrading precision must never be mistaken for a different value: every
// buffer size from tiny to generous either fits a faithful number or says "*".
TEST(UnitConversion, EveryBufferSizeYieldsAFaithfulNumberOrAMarker) {
  for (size_t capacity = 2; capacity <= 32; ++capacity) {
    std::string buffer(capacity, '\0');
    units::format(1234567.89, buffer.data(), capacity);
    const std::string text(buffer.c_str());
    ASSERT_LT(text.size(), capacity) << "overran a " << capacity << "-byte buffer";
    if (text == "*" || text.empty()) continue;
    const double parsed = std::atof(text.c_str());
    EXPECT_NEAR(parsed / 1234567.89, 1.0, 0.10) << "capacity " << capacity << " produced a misleading value: " << text;
  }
}

TEST(UnitConversion, FormatHandlesDegenerateBuffers) {
  char one[1] = {'x'};
  units::format(1.0, one, sizeof(one));
  EXPECT_EQ(one[0], '\0');
  units::format(1.0, nullptr, 16);  // must not crash
}

}  // namespace
