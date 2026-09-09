add_executable(CipherTest
  CipherTest.cpp
  ${REPO_ROOT}/src/activities/apps/cipher/Cipher.cpp
)

target_link_libraries(CipherTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

target_include_directories(CipherTest PRIVATE
  ${REPO_ROOT}/src/activities/apps/cipher
)

gtest_discover_tests(CipherTest)
