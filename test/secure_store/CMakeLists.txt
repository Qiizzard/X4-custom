# SecureStore's crypto comes from mbedtls, which the firmware already links for
# OTA SHA-256. The host test fetches a pinned mbedtls rather than relying on a
# system libmbedtls-dev, so CI and a developer laptop exercise the same version
# the device does (ESP-IDF 5.x ships mbedtls 3.6.x).
include(FetchContent)

FetchContent_Declare(
  mbedtls
  GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
  # 3.5.2 rather than the 3.6.x the firmware links: from 3.6.1 mbedtls's
  # top-level CMakeLists hard-requires its `framework` submodule even with
  # ENABLE_TESTING=OFF, and cloning that submodule dominates the runtime of this
  # whole test suite. The APIs SecureStore uses -- mbedtls_gcm_*,
  # mbedtls_pkcs5_pbkdf2_hmac_ext, mbedtls_ctr_drbg_* -- are unchanged across
  # 3.x, so this tests the same code paths the device runs. If SecureStore ever
  # reaches for an API newer than 3.5, move this tag up and pay the submodule.
  GIT_TAG mbedtls-3.5.2
  GIT_SHALLOW TRUE
  GIT_SUBMODULES ""
  GIT_SUBMODULES_RECURSE FALSE
)

set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(mbedtls)

add_executable(SecureStoreTest
  SecureStoreTest.cpp
  ${REPO_ROOT}/lib/SecureStore/SecureStore.cpp
)

target_link_libraries(SecureStoreTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
  MbedTLS::mbedtls
  MbedTLS::mbedcrypto
)

target_include_directories(SecureStoreTest PRIVATE
  ${REPO_ROOT}/lib/SecureStore
)

gtest_discover_tests(SecureStoreTest)
