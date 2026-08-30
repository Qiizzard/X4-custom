add_executable(RingBufferTest
  RingBufferTest.cpp
)

target_link_libraries(RingBufferTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

target_include_directories(RingBufferTest PRIVATE
  ${REPO_ROOT}/lib/RingBuffer
)

find_package(Threads REQUIRED)
target_link_libraries(RingBufferTest PRIVATE Threads::Threads)

gtest_discover_tests(RingBufferTest)
