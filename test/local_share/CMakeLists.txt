add_executable(LocalShareTest LocalShareTest.cpp ${REPO_ROOT}/lib/HalBulletinServer/HalBulletinServer.cpp)
target_include_directories(LocalShareTest PRIVATE stubs ../encrypted_files/stubs ${REPO_ROOT}/lib/HalBulletinServer)
target_link_libraries(LocalShareTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(LocalShareTest)
