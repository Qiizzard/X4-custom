#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <string>

#include "PasswordRecords.h"
#include "Totp.h"

TEST(VaultRecords, CapacityRoundTripAndAliasedEdit) {
  PasswordRecords records;
  for (size_t i = 0; i < PasswordRecords::kMaxRecords; ++i) ASSERT_TRUE(records.put(i, "entry", "network", "secret"));
  EXPECT_FALSE(records.put(8, "overflow", "", "secret"));
  const auto* first = records.at(0);
  ASSERT_TRUE(records.put(0, first->title, first->username, first->password));
  std::array<uint8_t, PasswordRecords::kEncodedBytes> bytes{};
  ASSERT_TRUE(records.encode(bytes.data(), bytes.size()));
  PasswordRecords decoded;
  ASSERT_TRUE(decoded.decode(bytes.data(), bytes.size()));
  ASSERT_EQ(decoded.size(), 8u);
  EXPECT_STREQ(decoded.at(0)->password, "secret");
  ASSERT_TRUE(decoded.erase(0));
  EXPECT_EQ(decoded.size(), 7u);
  EXPECT_EQ(decoded.at(7), nullptr);
  decoded.lock();
  EXPECT_EQ(decoded.size(), 0u);
  EXPECT_EQ(decoded.at(0), nullptr);
}
TEST(VaultRecords, InvalidDecodeClearsExistingPlaintext) {
  PasswordRecords records;
  ASSERT_TRUE(records.put(0, "entry", "ssid", "password"));
  std::array<uint8_t, PasswordRecords::kEncodedBytes> valid{};
  ASSERT_TRUE(records.encode(valid.data(), valid.size()));
  for (unsigned problem = 0; problem < 7; ++problem) {
    ASSERT_TRUE(records.decode(valid.data(), valid.size()));
    auto bytes = valid;
    size_t length = bytes.size();
    switch (problem) {
      case 0:
        bytes[0] = 'X';
        break;
      case 1:
        bytes[4] = 9;
        break;
      case 2:
        bytes[5] = 1;
        break;
      case 3:
        memset(bytes.data() + 8, 'a', 32);
        break;
      case 4:
        memset(bytes.data() + 8 + 80, 'x', 64);
        break;
      case 5:
        bytes[8 + 80] = 0;
        break;
      case 6:
        --length;
        break;
    }
    EXPECT_FALSE(records.decode(bytes.data(), length));
    EXPECT_EQ(records.size(), 0u);
    EXPECT_EQ(records.at(0), nullptr);
  }
}
TEST(VaultRecords, LengthLimitsAndPaddingNormalization) {
  PasswordRecords records;
  ASSERT_TRUE(records.put(0, std::string(31, 't').c_str(), std::string(47, 'u').c_str(), std::string(63, 'p').c_str()));
  EXPECT_FALSE(records.put(1, std::string(32, 't').c_str(), "", "p"));
  EXPECT_FALSE(records.put(1, "t", std::string(48, 'u').c_str(), "p"));
  EXPECT_FALSE(records.put(1, "t", "", std::string(64, 'p').c_str()));
  ASSERT_TRUE(records.put(0, "t", "", "p"));
  std::array<uint8_t, PasswordRecords::kEncodedBytes> bytes{};
  ASSERT_TRUE(records.encode(bytes.data(), bytes.size()));
  bytes[8 + 3] = 0x7f;  // Junk beyond the title NUL must not survive re-encoding.
  ASSERT_TRUE(records.decode(bytes.data(), bytes.size()));
  ASSERT_TRUE(records.encode(bytes.data(), bytes.size()));
  EXPECT_EQ(bytes[8 + 3], 0);
}
TEST(Totp, Rfc6238Sha1VectorsReducedToSixDigits) {
  // RFC's 20-byte ASCII key 12345678901234567890, encoded as canonical Base32.
  constexpr char seed[] = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";
  struct Vector {
    uint64_t time;
    const char* code;
  };
  constexpr Vector vectors[] = {{59, "287082"},         {1111111109, "081804"}, {1111111111, "050471"},
                                {1234567890, "005924"}, {2000000000, "279037"}, {20000000000ULL, "353130"}};
  Totp totp;
  ASSERT_TRUE(totp.open());
  char code[7]{};
  for (const auto& vector : vectors) {
    ASSERT_TRUE(totp.generate(seed, vector.time, code));
    EXPECT_STREQ(code, vector.code);
  }
  totp.close();
  EXPECT_FALSE(totp.generate(seed, 59, code));
  EXPECT_STREQ(code, "");
}
TEST(Totp, RejectsInvalidBase32AndHandlesCanonicalCase) {
  EXPECT_TRUE(Totp::validSeed("gezdgnbvgy3tqojq"));
  EXPECT_FALSE(Totp::validSeed("GEZDGNBVGY3TQOJ0"));
  EXPECT_FALSE(Totp::validSeed("GEZDGNBVGY3TQOJQ="));
  EXPECT_FALSE(Totp::validSeed("GEZD GN BVGY3TQOJQ"));
  EXPECT_FALSE(Totp::validSeed("A"));
  EXPECT_FALSE(Totp::validSeed("AAAAAAAAAAAAAAAAAB"));  // nonzero unused tail bits
  EXPECT_TRUE(Totp::validSeed("AAAAAAAAAAAAAAAAAA"));
}
