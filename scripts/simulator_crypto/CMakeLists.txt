cmake_minimum_required(VERSION 3.16)
project(crossink_simulator_crypto C)
include(FetchContent)
# Same pinned host implementation as test/secure_store; no simulator crypto stubs.
FetchContent_Declare(mbedtls
  GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
  GIT_TAG mbedtls-3.5.2
  GIT_SHALLOW TRUE
  UPDATE_DISCONNECTED TRUE
  GIT_SUBMODULES ""
  GIT_SUBMODULES_RECURSE FALSE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(mbedtls)
