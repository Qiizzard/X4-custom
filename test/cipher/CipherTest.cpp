// Host tests for cipher::apply(). These are the transforms a screenshot can't
// verify -- a cipher that's quietly wrong looks identical to one that's right
// (the same rationale as test/morse_code/).
#include <gtest/gtest.h>

#include <cctype>
#include <cstring>

#include "Cipher.h"

namespace {

std::string applyStr(const cipher::Algorithm algo, const char* input, const char* key = "") {
  char out[512];
  const size_t n = cipher::apply(algo, input, key, out, sizeof(out));
  return n == 0 ? std::string() : std::string(out, n);
}

TEST(Cipher, Rot13RoundTrips) {
  const std::string once = applyStr(cipher::Algorithm::Rot13, "Hello, World!");
  EXPECT_EQ(once, "Uryyb, Jbeyq!");
  const std::string twice = applyStr(cipher::Algorithm::Rot13, once.c_str());
  EXPECT_EQ(twice, "Hello, World!");
}

TEST(Cipher, CaesarShiftsAndWrapsAlphabet) {
  EXPECT_EQ(applyStr(cipher::Algorithm::Caesar, "xyz", "3"), "abc");
  EXPECT_EQ(applyStr(cipher::Algorithm::Caesar, "abc", "-3"), "xyz");
  // Non-numeric key parses to shift 0 -- a no-op, not a crash.
  EXPECT_EQ(applyStr(cipher::Algorithm::Caesar, "abc", "not-a-number"), "abc");
}

TEST(Cipher, CaesarPreservesCaseAndNonAlpha) {
  EXPECT_EQ(applyStr(cipher::Algorithm::Caesar, "Attack at Dawn! 123", "1"), "Buubdl bu Ebxo! 123");
}

TEST(Cipher, VigenereRoundTrips) {
  const std::string encoded = applyStr(cipher::Algorithm::Vigenere, "ATTACKATDAWN", "LEMON");
  EXPECT_EQ(encoded, "LXFOPVEFRNHR");
}

TEST(Cipher, VigenereRejectsKeyWithNoLetters) {
  char out[64];
  EXPECT_EQ(cipher::apply(cipher::Algorithm::Vigenere, "hello", "1234", out, sizeof(out)), 0u);
  EXPECT_EQ(cipher::apply(cipher::Algorithm::Vigenere, "hello", "", out, sizeof(out)), 0u);
}

TEST(Cipher, AtbashIsSelfInverse) {
  const std::string once = applyStr(cipher::Algorithm::Atbash, "Attack");
  EXPECT_EQ(once, "Zggzxp");
  EXPECT_EQ(applyStr(cipher::Algorithm::Atbash, once.c_str()), "Attack");
}

TEST(Cipher, XorHexRoundTrips) {
  const std::string cipherText = applyStr(cipher::Algorithm::XorEncryptHex, "Attack at dawn", "key");
  // Output must be plain hex -- never raw ciphertext bytes, which could be
  // unprintable and corrupt the e-ink text renderer.
  for (const char c : cipherText) {
    EXPECT_TRUE(std::isxdigit(static_cast<unsigned char>(c)));
  }
  EXPECT_EQ(applyStr(cipher::Algorithm::XorDecryptHex, cipherText.c_str(), "key"), "Attack at dawn");
}

TEST(Cipher, XorDecryptRejectsNonHex) {
  char out[64];
  EXPECT_EQ(cipher::apply(cipher::Algorithm::XorDecryptHex, "not hex!", "key", out, sizeof(out)), 0u);
  EXPECT_EQ(cipher::apply(cipher::Algorithm::XorDecryptHex, "abc", "key", out, sizeof(out)), 0u);  // odd length
}

TEST(Cipher, XorRequiresAKey) {
  char out[64];
  EXPECT_EQ(cipher::apply(cipher::Algorithm::XorEncryptHex, "hello", "", out, sizeof(out)), 0u);
  EXPECT_EQ(cipher::apply(cipher::Algorithm::XorDecryptHex, "68656c6c6f", "", out, sizeof(out)), 0u);
}

TEST(Cipher, Base64RoundTrips) {
  EXPECT_EQ(applyStr(cipher::Algorithm::Base64Encode, "Man"), "TWFu");
  EXPECT_EQ(applyStr(cipher::Algorithm::Base64Encode, "Ma"), "TWE=");
  EXPECT_EQ(applyStr(cipher::Algorithm::Base64Encode, "M"), "TQ==");
  EXPECT_EQ(applyStr(cipher::Algorithm::Base64Decode, "TWFu"), "Man");
  EXPECT_EQ(applyStr(cipher::Algorithm::Base64Decode, "TWE="), "Ma");
  EXPECT_EQ(applyStr(cipher::Algorithm::Base64Decode, "TQ=="), "M");
}

TEST(Cipher, Base64DecodeRejectsGarbageInsteadOfSkippingIt) {
  // biscuit's original silently skipped invalid characters, which turns a
  // mistyped ciphertext into a different, plausible-looking plaintext instead
  // of a visible error. This must fail closed.
  char out[64];
  EXPECT_EQ(cipher::apply(cipher::Algorithm::Base64Decode, "not valid base64!!", "", out, sizeof(out)), 0u);
  EXPECT_EQ(cipher::apply(cipher::Algorithm::Base64Decode, "abc", "", out, sizeof(out)), 0u);  // not a multiple of 4
}

TEST(Cipher, OverflowReportsFailureRatherThanTruncating) {
  char tiny[4];
  EXPECT_EQ(cipher::apply(cipher::Algorithm::Rot13, "this will not fit", "", tiny, sizeof(tiny)), 0u);
  EXPECT_STREQ(tiny, "");
}

TEST(Cipher, EmptyInputProducesEmptyResult) {
  EXPECT_EQ(applyStr(cipher::Algorithm::Rot13, ""), "");
  EXPECT_EQ(applyStr(cipher::Algorithm::Base64Encode, ""), "");
}

TEST(Cipher, NeedsKeyMatchesTheAlgorithmsThatUseOne) {
  EXPECT_FALSE(cipher::needsKey(cipher::Algorithm::Rot13));
  EXPECT_TRUE(cipher::needsKey(cipher::Algorithm::Caesar));
  EXPECT_TRUE(cipher::needsKey(cipher::Algorithm::Vigenere));
  EXPECT_TRUE(cipher::needsKey(cipher::Algorithm::XorEncryptHex));
  EXPECT_TRUE(cipher::needsKey(cipher::Algorithm::XorDecryptHex));
  EXPECT_FALSE(cipher::needsKey(cipher::Algorithm::Atbash));
  EXPECT_FALSE(cipher::needsKey(cipher::Algorithm::Base64Encode));
  EXPECT_FALSE(cipher::needsKey(cipher::Algorithm::Base64Decode));
}

TEST(Cipher, CaesarLargeKeysStayBounded) {
  EXPECT_EQ(applyStr(cipher::Algorithm::Caesar, "Zz", "2147483647"), "Ww");
  EXPECT_EQ(applyStr(cipher::Algorithm::Caesar, "Az", "99999999999999999999999999999999"), "Vu");
  EXPECT_EQ(applyStr(cipher::Algorithm::Caesar, "Az", "-99999999999999999999999999999999"), "Fe");
}

TEST(Cipher, MalformedDecodeClearsPartialOutput) {
  char out[64] = {};
  EXPECT_EQ(cipher::apply(cipher::Algorithm::XorDecryptHex, "414G", "k", out, sizeof(out)), 0u);
  EXPECT_STREQ(out, "");
  EXPECT_EQ(cipher::apply(cipher::Algorithm::Base64Decode, "TW!u", "", out, sizeof(out)), 0u);
  EXPECT_STREQ(out, "");
}

}  // namespace
