add_executable(EncryptedFilesTest
  EncryptedFilesTest.cpp
  ${REPO_ROOT}/src/activities/apps/secure_vault/EncryptedVaultFile.cpp
  ${REPO_ROOT}/src/activities/apps/stego_notes/EncryptedBmp.cpp
  ${REPO_ROOT}/lib/SecureStore/SecureStore.cpp)
target_include_directories(EncryptedFilesTest PRIVATE stubs
  ${REPO_ROOT}/src/activities/apps/secure_vault
  ${REPO_ROOT}/src/activities/apps/stego_notes ${REPO_ROOT}/lib/SecureStore)
target_link_libraries(EncryptedFilesTest PRIVATE crosspoint_test_common GTest::gtest_main MbedTLS::mbedcrypto)
gtest_discover_tests(EncryptedFilesTest)
