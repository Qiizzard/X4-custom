add_executable(UnitConversionTest
  UnitConversionTest.cpp
  ${REPO_ROOT}/src/activities/apps/unit_converter/UnitConversion.cpp
)

target_link_libraries(UnitConversionTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

target_include_directories(UnitConversionTest PRIVATE
  ${REPO_ROOT}/src/activities/apps/unit_converter
)

gtest_discover_tests(UnitConversionTest)
