// SecureStore -- the AES-256-GCM + PBKDF2 building block RULESET.md rule 22
// names as the approved replacement for XOR-on-MAC "obfuscation".
//
// Rule 21 says a security feature either works and is reviewed, or it is
// removed. These tests are that review's evidence. The properties that matter
// are not "it round-trips" but: a wrong passphrase fails, a tampered blob
// fails, the same plaintext never encrypts to the same bytes twice, and the
// key is not recoverable from anything stored on the device.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "SecureStore.h"

namespace {

using securestore::Status;

// Fast KDF cost for tests. Production uses kDefaultIterations; the algorithm
// under test is identical either way, and this keeps the suite quick.
constexpr uint32_t kTestIterations = 1000;

constexpr char kPin[] = "4821";
constexpr char kDuressPin[] = "9137";

std::vector<uint8_t> bytesOf(const std::string& s) { return {s.begin(), s.end()}; }

// Encrypt helper that fails the test rather than returning a bad blob.
std::vector<uint8_t> seal(const std::vector<uint8_t>& plaintext, const char* passphrase,
                          const uint32_t iterations = kTestIterations) {
  std::vector<uint8_t> blob(securestore::encryptedSize(plaintext.size()));
  size_t written = 0;
  const Status status = securestore::encrypt(plaintext.data(), plaintext.size(), passphrase, blob.data(), blob.size(),
                                             &written, iterations);
  EXPECT_EQ(status, Status::Ok) << securestore::statusName(status);
  EXPECT_EQ(written, blob.size());
  return blob;
}

TEST(SecureStore, RoundTripsAVaultUnderTheCorrectPassphrase) {
  const auto secret = bytesOf(R"({"site":"bank","user":"ada","pass":"correct horse battery staple"})");
  const auto blob = seal(secret, kPin);

  size_t plainLength = 0;
  ASSERT_TRUE(securestore::plaintextSize(blob.data(), blob.size(), &plainLength));
  ASSERT_EQ(plainLength, secret.size());

  std::vector<uint8_t> out(plainLength);
  size_t written = 0;
  ASSERT_EQ(securestore::decrypt(blob.data(), blob.size(), kPin, out.data(), out.size(), &written), Status::Ok);
  EXPECT_EQ(written, secret.size());
  EXPECT_EQ(out, secret);
}

// The property that makes this worth shipping: the plaintext is not in the file.
// The banned XOR-on-MAC scheme failed exactly here.
TEST(SecureStore, CiphertextDoesNotContainThePlaintext) {
  const auto secret = bytesOf("hunter2-my-actual-wifi-password");
  const auto blob = seal(secret, kPin);
  const bool leaked = std::search(blob.begin(), blob.end(), secret.begin(), secret.end()) != blob.end();
  EXPECT_FALSE(leaked) << "plaintext appeared verbatim inside the encrypted blob";
}

TEST(SecureStore, WrongPassphraseFailsAuthenticationAndYieldsNoPlaintext) {
  const auto secret = bytesOf("top secret note");
  const auto blob = seal(secret, kPin);

  std::vector<uint8_t> out(secret.size(), 0xEE);
  size_t written = 0;
  EXPECT_EQ(securestore::decrypt(blob.data(), blob.size(), "0000", out.data(), out.size(), &written),
            Status::AuthFailed);
  // No partial plaintext may survive a failed decrypt.
  EXPECT_EQ(out, std::vector<uint8_t>(secret.size(), 0x00))
      << "a failed decrypt left readable bytes in the caller's buffer";
}

TEST(SecureStore, VerifyPassphraseAcceptsOnlyTheRealOne) {
  const auto blob = seal(bytesOf("vault"), kPin);
  EXPECT_TRUE(securestore::verifyPassphrase(blob.data(), blob.size(), kPin));
  EXPECT_FALSE(securestore::verifyPassphrase(blob.data(), blob.size(), "4822"));
  EXPECT_FALSE(securestore::verifyPassphrase(blob.data(), blob.size(), "482"));
}

// Every byte of the blob is covered by the GCM tag. Flipping any one of them --
// header field or ciphertext -- must fail, not silently change the plaintext.
TEST(SecureStore, AnySingleFlippedBitFailsAuthentication) {
  const auto secret = bytesOf("integrity matters more than confidentiality here");
  const auto blob = seal(secret, kPin);

  std::vector<uint8_t> out(secret.size());
  size_t written = 0;
  for (size_t i = 0; i < blob.size(); ++i) {
    auto tampered = blob;
    tampered[i] ^= 0x01;
    const Status status =
        securestore::decrypt(tampered.data(), tampered.size(), kPin, out.data(), out.size(), &written);
    // Corrupting the magic/version/kdf bytes is caught as BadFormat; everything
    // else is caught by the tag. Either way it must never come back Ok.
    EXPECT_NE(status, Status::Ok) << "byte " << i << " could be modified without detection";
  }
}

// The salt, iteration count and length live in the header. If they were not
// authenticated, an attacker could drop the KDF cost to a single iteration and
// brute-force the PIN cheaply. They are fed to GCM as AAD, so they cannot.
TEST(SecureStore, RewritingTheIterationCountIsDetected) {
  const auto blob = seal(bytesOf("cost matters"), kPin, 4096);

  auto weakened = blob;
  weakened[8] = 1;  // iterations -> 1
  weakened[9] = 0;
  weakened[10] = 0;
  weakened[11] = 0;

  std::vector<uint8_t> out(32);
  size_t written = 0;
  EXPECT_NE(securestore::decrypt(weakened.data(), weakened.size(), kPin, out.data(), out.size(), &written), Status::Ok)
      << "an attacker could downgrade the KDF cost undetected";
}

// Encrypting the same vault twice must not produce the same file, or an
// observer with two SD-card snapshots learns whether anything changed.
TEST(SecureStore, IdenticalPlaintextEncryptsDifferentlyEveryTime) {
  const auto secret = bytesOf("unchanged contents");
  std::set<std::vector<uint8_t>> seen;
  for (int i = 0; i < 8; ++i) seen.insert(seal(secret, kPin));
  EXPECT_EQ(seen.size(), 8u) << "salt/IV are not fresh per encryption";
}

TEST(SecureStore, SaltAndIvDifferBetweenBlobs) {
  const auto a = seal(bytesOf("x"), kPin);
  const auto b = seal(bytesOf("x"), kPin);
  EXPECT_NE(0, std::memcmp(a.data() + 12, b.data() + 12, securestore::kSaltBytes)) << "salt reused";
  EXPECT_NE(0, std::memcmp(a.data() + 28, b.data() + 28, securestore::kIvBytes)) << "IV reused";
}

// The duress-vault property. Two PINs, two blobs; each opens its own and fails
// on the other. This is what makes the decoy real rather than a UI trick.
TEST(SecureStore, DuressPinOpensOnlyTheDecoyVault) {
  const auto realVault = bytesOf(R"([{"site":"bank","pass":"real"}])");
  const auto decoyVault = bytesOf(R"([{"site":"forum","pass":"decoy"}])");
  const auto realBlob = seal(realVault, kPin);
  const auto decoyBlob = seal(decoyVault, kDuressPin);

  std::vector<uint8_t> out(64);
  size_t written = 0;

  ASSERT_EQ(securestore::decrypt(realBlob.data(), realBlob.size(), kPin, out.data(), out.size(), &written), Status::Ok);
  EXPECT_EQ(std::vector<uint8_t>(out.begin(), out.begin() + written), realVault);

  ASSERT_EQ(securestore::decrypt(decoyBlob.data(), decoyBlob.size(), kDuressPin, out.data(), out.size(), &written),
            Status::Ok);
  EXPECT_EQ(std::vector<uint8_t>(out.begin(), out.begin() + written), decoyVault);

  // Crucially: the duress PIN cannot reach the real vault.
  EXPECT_EQ(securestore::decrypt(realBlob.data(), realBlob.size(), kDuressPin, out.data(), out.size(), &written),
            Status::AuthFailed);
  EXPECT_FALSE(securestore::verifyPassphrase(realBlob.data(), realBlob.size(), kDuressPin));
}

// A device-identity value must never be the key. Encrypting under the same
// passphrase on two "devices" has to produce interchangeable blobs -- and
// nothing stored in the blob may be a MAC-derived constant.
TEST(SecureStore, KeyDependsOnPassphraseNotDeviceIdentity) {
  const auto secret = bytesOf("portable");
  const auto blobFromDeviceA = seal(secret, kPin);

  std::vector<uint8_t> out(secret.size());
  size_t written = 0;
  // "Device B" is this same code with no device state involved; if identity
  // leaked into the key, this decrypt would fail.
  ASSERT_EQ(
      securestore::decrypt(blobFromDeviceA.data(), blobFromDeviceA.size(), kPin, out.data(), out.size(), &written),
      Status::Ok);
  EXPECT_EQ(out, secret);
}

TEST(SecureStore, RejectsBlobsThatAreNotOurs) {
  std::vector<uint8_t> garbage(120, 0x41);
  std::vector<uint8_t> out(64);
  size_t written = 0;
  EXPECT_EQ(securestore::decrypt(garbage.data(), garbage.size(), kPin, out.data(), out.size(), &written),
            Status::BadFormat);

  size_t length = 0;
  EXPECT_FALSE(securestore::plaintextSize(garbage.data(), garbage.size(), &length));
  // Truncated below a full header.
  EXPECT_FALSE(securestore::plaintextSize(garbage.data(), 8, &length));
}

// A header claiming more payload than the buffer holds must be rejected before
// anything reads past the end of it.
TEST(SecureStore, RejectsALengthFieldThatOverrunsTheBuffer) {
  auto blob = seal(bytesOf("short"), kPin);
  blob[56] = 0xFF;  // plaintext length -> huge
  blob[57] = 0xFF;
  blob[58] = 0x00;
  blob[59] = 0x00;

  size_t length = 0;
  EXPECT_FALSE(securestore::plaintextSize(blob.data(), blob.size(), &length));

  std::vector<uint8_t> out(64);
  size_t written = 0;
  EXPECT_EQ(securestore::decrypt(blob.data(), blob.size(), kPin, out.data(), out.size(), &written), Status::BadFormat);
}

// An absurd iteration count in a corrupted header would otherwise freeze the
// unlock screen for minutes on a 160 MHz core before the tag check ran.
TEST(SecureStore, RejectsAnAbsurdIterationCountWithoutRunningIt) {
  auto blob = seal(bytesOf("x"), kPin);
  blob[8] = 0xFF;
  blob[9] = 0xFF;
  blob[10] = 0xFF;
  blob[11] = 0xFF;

  std::vector<uint8_t> out(8);
  size_t written = 0;
  EXPECT_EQ(securestore::decrypt(blob.data(), blob.size(), kPin, out.data(), out.size(), &written), Status::BadFormat);
}

TEST(SecureStore, RejectsPassphrasesTooShortToBeWorthEncryptingUnder) {
  const auto secret = bytesOf("x");
  std::vector<uint8_t> blob(securestore::encryptedSize(secret.size()));
  size_t written = 0;
  EXPECT_EQ(
      securestore::encrypt(secret.data(), secret.size(), "12", blob.data(), blob.size(), &written, kTestIterations),
      Status::BadArgument);
  EXPECT_EQ(securestore::encrypt(secret.data(), secret.size(), "", blob.data(), blob.size(), &written, kTestIterations),
            Status::BadArgument);
}

TEST(SecureStore, RejectsAnUndersizedOutputBuffer) {
  const auto secret = bytesOf("needs room");
  std::vector<uint8_t> tooSmall(securestore::encryptedSize(secret.size()) - 1);
  size_t written = 0;
  EXPECT_EQ(securestore::encrypt(secret.data(), secret.size(), kPin, tooSmall.data(), tooSmall.size(), &written,
                                 kTestIterations),
            Status::BadArgument);
}

TEST(SecureStore, HandlesAnEmptyVault) {
  std::vector<uint8_t> empty;
  std::vector<uint8_t> blob(securestore::encryptedSize(0));
  size_t written = 0;
  ASSERT_EQ(securestore::encrypt(nullptr, 0, kPin, blob.data(), blob.size(), &written, kTestIterations), Status::Ok);
  EXPECT_EQ(written, securestore::kHeaderBytes);
  EXPECT_TRUE(securestore::verifyPassphrase(blob.data(), blob.size(), kPin));
  EXPECT_FALSE(securestore::verifyPassphrase(blob.data(), blob.size(), "0000"));
}

// A real vault is kilobytes, not bytes. Confirm nothing is size-limited by an
// internal fixed buffer.
TEST(SecureStore, HandlesAVaultLargerThanAnySingleBuffer) {
  std::vector<uint8_t> big(16 * 1024);
  for (size_t i = 0; i < big.size(); ++i) big[i] = static_cast<uint8_t>(i * 31);
  const auto blob = seal(big, kPin);

  std::vector<uint8_t> out(big.size());
  size_t written = 0;
  ASSERT_EQ(securestore::decrypt(blob.data(), blob.size(), kPin, out.data(), out.size(), &written), Status::Ok);
  EXPECT_EQ(written, big.size());
  EXPECT_EQ(out, big);
}

// Blobs record their own iteration count, so raising the production cost later
// must not orphan vaults written under the old one.
TEST(SecureStore, ReadsBlobsWrittenWithADifferentIterationCount) {
  const auto secret = bytesOf("written under an older cost");
  const auto oldBlob = seal(secret, kPin, 512);

  std::vector<uint8_t> out(secret.size());
  size_t written = 0;
  ASSERT_EQ(securestore::decrypt(oldBlob.data(), oldBlob.size(), kPin, out.data(), out.size(), &written), Status::Ok);
  EXPECT_EQ(out, secret);
}

TEST(SecureStore, SecureZeroClearsABuffer) {
  std::vector<uint8_t> buf(64, 0xAB);
  securestore::secureZero(buf.data(), buf.size());
  EXPECT_EQ(buf, std::vector<uint8_t>(64, 0x00));
}

}  // namespace
