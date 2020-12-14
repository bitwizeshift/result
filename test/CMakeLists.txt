find_package(Catch2 REQUIRED)

set(source_files
  src/main.cpp
  src/result.test.cpp
  src/result.constexpr.test.cpp
  src/failure.test.cpp
)

add_executable(${PROJECT_NAME}.test
  ${source_files}
)
add_executable(${PROJECT_NAME}::test ALIAS ${PROJECT_NAME}.test)

target_link_libraries(${PROJECT_NAME}.test
  PRIVATE ${PROJECT_NAME}::${PROJECT_NAME}
  PRIVATE Catch2::Catch2
)

set_target_properties(${UNITTEST_TARGET_NAME} PROPERTIES
  CXX_STANDARD 11
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" AND
    "${CMAKE_CXX_SIMULATE_ID}" STREQUAL "MSVC")
  target_compile_options(${PROJECT_NAME}.test PRIVATE
    -Wno-sign-conversion
    -Wno-shorten-64-to-32
    -Wno-unneeded-member-function
    -Wno-unused-member-function
  )
endif ()

##############################################################################
# CTest
##############################################################################

include(Catch)
catch_discover_tests(${PROJECT_NAME}.test)
