cmake_minimum_required(VERSION 3.8)

project(Result_tests CXX)

find_package(Result CONFIG REQUIRED)

find_package(Catch2 REQUIRED)
include(Catch)

add_executable(
    Result.test
    src/main.cpp
    src/result.test.cpp
    src/result.constexpr.test.cpp
    src/failure.test.cpp
)
target_link_libraries(Result.test PRIVATE Result::Result Catch2::Catch2)
target_compile_features(Result.test PRIVATE cxx_std_11)

if(
    CMAKE_CXX_COMPILER_ID MATCHES "Clang"
    AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC"
)
  target_compile_options(
      Result.test
      PRIVATE
      -Wno-sign-conversion
      -Wno-shorten-64-to-32
      -Wno-unneeded-member-function
      -Wno-unused-member-function
  )
endif()

enable_testing()
catch_discover_tests(Result.test)
