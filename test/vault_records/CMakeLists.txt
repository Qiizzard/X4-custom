add_executable(VaultRecordsTest
  VaultRecordsTest.cpp
  ${REPO_ROOT}/src/activities/apps/secure_vault/PasswordRecords.cpp
  ${REPO_ROOT}/src/activities/apps/secure_vault/Totp.cpp
  ${REPO_ROOT}/lib/SecureStore/SecureStore.cpp)
target_include_directories(VaultRecordsTest PRIVATE
  stubs ${REPO_ROOT}/src/activities/apps/secure_vault ${REPO_ROOT}/lib/SecureStore)
target_link_libraries(VaultRecordsTest PRIVATE crosspoint_test_common GTest::gtest_main MbedTLS::mbedcrypto)
gtest_discover_tests(VaultRecordsTest)
