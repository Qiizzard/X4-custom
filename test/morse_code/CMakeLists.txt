add_executable(MorseCodeTest
  MorseCodeTest.cpp
  ${REPO_ROOT}/src/activities/apps/morse_code/MorseCode.cpp
)

target_link_libraries(MorseCodeTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

target_include_directories(MorseCodeTest PRIVATE
  ${REPO_ROOT}/src/activities/apps/morse_code
)

gtest_discover_tests(MorseCodeTest)
