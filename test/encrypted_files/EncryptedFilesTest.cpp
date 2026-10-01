#include <HalStorage.h>
#include <gtest/gtest.h>

#include <array>

#include "EncryptedBmp.h"
#include "EncryptedVaultFile.h"

namespace {
constexpr char path[] = "/crossink/vaults/test.bin", key[] = "test key only";
constexpr char carrier[] = "/crossink/drawings/test.bmp", hidden[] = "/crossink/stego/test.bmp";
class Files : public testing::Test {
 protected:
  std::array<uint8_t, vaultfile::kMaxPlaintext> plain{};
  std::array<uint8_t, vaultfile::kMaxBlob> scratch{};
  stegobmp::Workspace workspace;
  void SetUp() override { Storage = {}; }
  void TearDown() override { EXPECT_EQ(Storage.handles, 0u); }
  template <typename Buffer>
  void zero(const Buffer& b) {
    EXPECT_TRUE(std::all_of(std::begin(b), std::end(b), [](auto c) { return c == 0; }));
  }
  void bmp() {
    auto& bytes = Storage.files[carrier];
    bytes.resize(182, 0);
    bytes[0] = 'B';
    bytes[1] = 'M';
    bytes[2] = 182;
    bytes[10] = 54;
    bytes[14] = 40;
  }
};
TEST_F(Files, VaultRoundTripMaximumAndEmptyPreservesExclusiveDestination) {
  for (size_t length : {size_t(0), vaultfile::kMaxPlaintext}) {
    Storage.files.clear();
    plain.fill(0x5a);
    ASSERT_EQ(vaultfile::create(path, key, plain.data(), length, scratch.data(), scratch.size()).status,
              vaultfile::Status::Ok);
    zero(scratch);
    const auto original = Storage.files.at(path);
    EXPECT_EQ(vaultfile::create(path, key, plain.data(), length, scratch.data(), scratch.size()).status,
              vaultfile::Status::AlreadyExists);
    EXPECT_EQ(Storage.files.at(path), original);
    plain.fill(0);
    size_t out = 9;
    ASSERT_EQ(vaultfile::load(path, key, plain.data(), plain.size(), &out, scratch.data(), scratch.size()).status,
              vaultfile::Status::Ok);
    EXPECT_EQ(out, length);
    EXPECT_TRUE(std::all_of(plain.begin(), plain.begin() + length, [](auto c) { return c == 0x5a; }));
    zero(scratch);
  }
}
TEST_F(Files, VaultWrongKeyTamperAndIoFailuresWipeOutputs) {
  ASSERT_EQ(vaultfile::create(path, key, plain.data(), 128, scratch.data(), scratch.size()).status,
            vaultfile::Status::Ok);
  const auto original = Storage.files.at(path);
  for (unsigned scenario = 0; scenario < 5; ++scenario) {
    Storage.files[path] = original;
    Storage.fault = FakeStorage::Fault::None;
    if (scenario == 1) Storage.files[path].back() ^= 1;
    if (scenario == 2) Storage.fault = FakeStorage::Fault::Read;
    if (scenario == 3) Storage.fault = FakeStorage::Fault::Close;
    if (scenario == 4) Storage.files[path].resize(10);
    plain.fill(0xaa);
    scratch.fill(0xbb);
    size_t length = 99;
    const auto result = vaultfile::load(path, scenario == 0 ? "wrong key" : key, plain.data(), plain.size(), &length,
                                        scratch.data(), scratch.size());
    EXPECT_NE(result.status, vaultfile::Status::Ok);
    EXPECT_EQ(length, 0u);
    zero(plain);
    zero(scratch);
  }
}
TEST_F(Files, VaultWriteSyncCloseFailuresRemoveOnlyNewOutput) {
  for (auto fault : {FakeStorage::Fault::Write, FakeStorage::Fault::Sync, FakeStorage::Fault::Close}) {
    Storage = {};
    Storage.files["untouched"] = {1, 2, 3};
    Storage.fault = fault;
    EXPECT_EQ(vaultfile::create(path, key, plain.data(), 128, scratch.data(), scratch.size()).status,
              vaultfile::Status::IoError);
    EXPECT_FALSE(Storage.exists(path));
    EXPECT_EQ(Storage.files["untouched"], (std::vector<uint8_t>{1, 2, 3}));
    zero(scratch);
  }
}
TEST_F(Files, VaultCleanupFailureRetainsUncertainFileAndRejectsTraversal) {
  Storage.fault = FakeStorage::Fault::Write;
  Storage.removeFails = true;
  EXPECT_EQ(vaultfile::create(path, key, plain.data(), 128, scratch.data(), scratch.size()).status,
            vaultfile::Status::IoError);
  EXPECT_TRUE(Storage.exists(path));
  Storage.fault = FakeStorage::Fault::None;
  size_t length = 1;
  EXPECT_NE(vaultfile::load(path, key, plain.data(), plain.size(), &length, scratch.data(), scratch.size()).status,
            vaultfile::Status::Ok);
  EXPECT_EQ(length, 0u);
  EXPECT_EQ(
      vaultfile::create("/crossink/vaults/../escape.bin", key, plain.data(), 8, scratch.data(), scratch.size()).status,
      vaultfile::Status::BadArgument);
}
TEST_F(Files, StegoRoundTripPreservesCarrierAndRejectsWrongKeyAndTamper) {
  bmp();
  const auto original = Storage.files.at(carrier);
  plain.fill(0x41);
  ASSERT_TRUE(stegobmp::hide(carrier, hidden, key, plain.data(), 512, workspace));
  zero(workspace.blob);
  EXPECT_EQ(Storage.files.at(carrier), original);
  EXPECT_TRUE(std::equal(original.begin(), original.end(), Storage.files.at(hidden).begin()));
  std::array<uint8_t, 512> output{};
  size_t length = 0;
  ASSERT_TRUE(stegobmp::reveal(hidden, key, output.data(), output.size(), length, workspace));
  EXPECT_EQ(length, 512u);
  EXPECT_TRUE(std::all_of(output.begin(), output.end(), [](auto c) { return c == 0x41; }));
  zero(workspace.blob);
  EXPECT_FALSE(stegobmp::reveal(hidden, "wrong key", output.data(), output.size(), length, workspace));
  EXPECT_EQ(length, 0u);
  zero(output);
  Storage.files[hidden].back() ^= 1;
  EXPECT_FALSE(stegobmp::reveal(hidden, key, output.data(), output.size(), length, workspace));
  zero(output);
  zero(workspace.blob);
}
TEST_F(Files, StegoRejectsMalformedCarrierAndPreservesExistingDestination) {
  bmp();
  Storage.files[carrier][2] = 53;
  EXPECT_FALSE(stegobmp::hide(carrier, hidden, key, plain.data(), 10, workspace));
  EXPECT_FALSE(Storage.exists(hidden));
  bmp();
  Storage.files[hidden] = {9, 8, 7};
  EXPECT_FALSE(stegobmp::hide(carrier, hidden, key, plain.data(), 10, workspace));
  EXPECT_EQ(Storage.files[hidden], (std::vector<uint8_t>{9, 8, 7}));
}
TEST_F(Files, StegoWriteSyncCloseFailuresCleanUpNewFile) {
  for (auto fault : {FakeStorage::Fault::Write, FakeStorage::Fault::Sync, FakeStorage::Fault::Close}) {
    Storage = {};
    bmp();
    const auto original = Storage.files.at(carrier);
    Storage.fault = fault;
    EXPECT_FALSE(stegobmp::hide(carrier, hidden, key, plain.data(), 10, workspace));
    EXPECT_FALSE(Storage.exists(hidden));
    EXPECT_EQ(Storage.files.at(carrier), original);
    zero(workspace.blob);
  }
}
}  // namespace
