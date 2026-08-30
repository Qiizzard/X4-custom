// Morse encode/decode. Split out of the activity precisely so it can be tested
// here: a converter that is quietly wrong looks identical on screen to one that
// is right.
#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "MorseCode.h"

namespace {

std::string encoded(const std::string& text, const size_t capacity = 256) {
  std::string out(capacity, '\0');
  const size_t written = morse::encode(text.c_str(), out.data(), capacity);
  out.resize(written);
  return out;
}

std::string decoded(const std::string& code, const size_t capacity = 256) {
  std::string out(capacity, '\0');
  const size_t written = morse::decode(code.c_str(), out.data(), capacity);
  out.resize(written);
  return out;
}

TEST(MorseCode, EncodesTheClassicDistressSignal) { EXPECT_EQ(encoded("SOS"), "... --- ..."); }

TEST(MorseCode, EncodesLettersAndDigits) {
  EXPECT_EQ(encoded("A"), ".-");
  EXPECT_EQ(encoded("Z"), "--..");
  EXPECT_EQ(encoded("0"), "-----");
  EXPECT_EQ(encoded("9"), "----.");
  EXPECT_EQ(encoded("E"), ".");
}

TEST(MorseCode, IsCaseInsensitive) { EXPECT_EQ(encoded("sos"), encoded("SOS")); }

TEST(MorseCode, MarksWordBreaksWithSlash) { EXPECT_EQ(encoded("HI BOB"), ".... .. / -... --- -..."); }

TEST(MorseCode, SkipsCharactersWithNoCode) {
  // Punctuation is not in the table; it is dropped rather than mis-encoded.
  EXPECT_EQ(encoded("A!B"), ".- -...");
}

TEST(MorseCode, DecodesBackToTheOriginal) {
  EXPECT_EQ(decoded("... --- ..."), "SOS");
  EXPECT_EQ(decoded(".... .. / -... --- -..."), "HI BOB");
}

// The property that matters most for a converter: encode then decode is the
// identity on anything the table covers.
TEST(MorseCode, RoundTripsEveryCharacterInTheTable) {
  const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  for (const char c : alphabet) {
    const std::string one(1, c);
    EXPECT_EQ(decoded(encoded(one)), one) << "round trip failed for '" << c << "'";
  }
  EXPECT_EQ(decoded(encoded(alphabet)), alphabet);
}

TEST(MorseCode, RoundTripsAPhraseWithWordBreaks) {
  const std::string phrase = "THE QUICK BROWN FOX 1234";
  EXPECT_EQ(decoded(encoded(phrase)), phrase);
}

TEST(MorseCode, UnknownCodeBecomesAQuestionMarkRatherThanVanishing) {
  // ....... is not a real code. Dropping it silently would make a decode look
  // successful while losing a character.
  EXPECT_EQ(decoded("... ....... ---"), "S?O");
}

TEST(MorseCode, ToleratesRaggedSpacing) {
  EXPECT_EQ(decoded("...   ---   ..."), "SOS");
  EXPECT_EQ(decoded("  ... --- ...  "), "SOS");
}

// Truncating would produce a valid-looking but wrong message, so encode reports
// failure instead. The activity shows "too long" on this.
TEST(MorseCode, RefusesToTruncateRatherThanEmitAWrongMessage) {
  char small[8] = {};
  EXPECT_EQ(morse::encode("SOS SOS SOS", small, sizeof(small)), 0u);
  EXPECT_STREQ(small, "") << "a partial encoding was left in the buffer";
}

TEST(MorseCode, RefusesToTruncateOnDecodeToo) {
  char small[4] = {};
  EXPECT_EQ(morse::decode("... --- ... .-.. .-..", small, sizeof(small)), 0u);
  EXPECT_STREQ(small, "");
}

TEST(MorseCode, HandlesEmptyAndNullInput) {
  char out[16] = {'x'};
  EXPECT_EQ(morse::encode("", out, sizeof(out)), 0u);
  EXPECT_STREQ(out, "");
  EXPECT_EQ(morse::encode(nullptr, out, sizeof(out)), 0u);
  EXPECT_EQ(morse::decode("", out, sizeof(out)), 0u);
  EXPECT_EQ(morse::decode(nullptr, out, sizeof(out)), 0u);
  EXPECT_EQ(morse::encode("SOS", nullptr, 16), 0u);
  EXPECT_EQ(morse::encode("SOS", out, 0), 0u);
}

TEST(MorseCode, CapacityHelperIsEnoughForTheWorstCase) {
  // Six 5-symbol codes plus separators is the densest real input.
  const std::string worst = "000000";
  std::string out(morse::encodedCapacityFor(worst.size()), '\0');
  const size_t written = morse::encode(worst.c_str(), out.data(), out.size());
  EXPECT_GT(written, 0u) << "encodedCapacityFor() under-reported the space needed";
}

// The append helpers terminate as they go, so the buffer is a valid C string
// at every point rather than only once the caller writes the final byte.
TEST(MorseCode, LeavesAValidStringEvenPartwayThroughABuffer) {
  char buffer[64];
  std::memset(buffer, 'X', sizeof(buffer));
  const size_t written = morse::encode("SOS", buffer, sizeof(buffer));
  ASSERT_GT(written, 0u);
  EXPECT_EQ(std::strlen(buffer), written) << "buffer was not terminated at the reported length";
}

TEST(MorseCode, CodeForReportsUnknownCharacters) {
  EXPECT_STREQ(morse::codeFor('A'), ".-");
  EXPECT_STREQ(morse::codeFor('a'), ".-");
  EXPECT_EQ(morse::codeFor('!'), nullptr);
  EXPECT_EQ(morse::codeFor(' '), nullptr);
}

}  // namespace
