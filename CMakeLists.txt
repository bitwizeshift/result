cmake_minimum_required(VERSION 3.5)

if (PROJECT_NAME)
  set(IS_SUBPROJECT TRUE)
endif ()

set(EXPECTED_CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
set(EXPECTED_CMAKE_TEMPLATE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/templates")

set(CMAKE_MODULE_PATH "${EXPECTED_CMAKE_MODULE_PATH}" "${CMAKE_MODULE_PATH}")

option(EXPECTED_COMPILE_UNIT_TESTS "Compile and run the unit tests for this library" OFF)

if (NOT CMAKE_TESTING_ENABLED AND EXPECTED_COMPILE_UNIT_TESTS)
  enable_testing()
endif ()

project(Expected
  VERSION "0.0.1"
  LANGUAGES CXX
)

set(EXPECTED_VERSION_MAJOR ${PROJECT_VERSION_MAJOR} CACHE INTERNAL "Major version of Expected")
set(EXPECTED_VERSION_MINOR ${PROJECT_VERSION_MINOR} CACHE INTERNAL "Minor version of Expected")
set(EXPECTED_VERSION_PATCH ${PROJECT_VERSION_PATCH} CACHE INTERNAL "Patch version of Expected")
set(EXPECTED_VERSION ${PROJECT_VERSION} CACHE INTERNAL "Version of Expected")

if (EXISTS "${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo.cmake")
  include("${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo.cmake")
  conan_set_find_paths()
endif ()

##############################################################################
# Targets
##############################################################################

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(header_files
  include/result.hpp
)

add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
  INTERFACE $<INSTALL_INTERFACE:include>
)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" AND
    "${CMAKE_CXX_SIMULATE_ID}" STREQUAL "MSVC")
  # clang-cl does not appear to implement '-pedantic' or 'pedantic-errors',
  # so this case needs to be handled specifically
  add_compile_options(-Wall -Werror -Wextra)

  # clang-cl treats '-Wall' like '-Weverything' currently; so there are a few
  # warnings we need to manually disable.

  # Disable unnecessary compatibility and 'newline-eof'. This is a modern C++
  # library -- so these serve no purpose
  add_compile_options(-Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-newline-eof)

  # Disable warnings on padding. This is a great warning, but not good when
  # coupled with -Werror
  add_compile_options(-Wno-padded)

  # Several utilities require static objects that have exit-time destructors,
  # including 'error_category' types from the standard.
  add_compile_options(-Wno-exit-time-destructors)

  # This is erroring on valid uses of the standard library, like std::lock_guard.
  add_compile_options(-Wno-ctad-maybe-unsupported)

  # This is a good warning *in general*, but not when using MSVC's standard
  # library, which requires defining "_" prefixed macros just to have it operate
  # like the C++ standard dictates.
  add_compile_options(-Wno-reserved-id-macro)

  # Don't warn on ignored attributes; this breaks builds using dllexport/dllimport on
  # classes that may also define inline functions
  add_compile_options(-Wno-ignored-attributes)

elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR
        "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  add_compile_options(-Wall -Werror -Wextra -pedantic -pedantic-errors)

  # Xcode 11.2 seems to enable -Wnewline-eof with '-Wextra'. Disable this,
  # since it's a legacy requirement not needed in C++11 onward.
  add_compile_options(-Wno-newline-eof)

  # Don't warn on ignored attributes; this breaks builds using dllexport/dllimport on
  # classes that may also define inline functions
  add_compile_options(-Wno-ignored-attributes)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  add_compile_options(/W4 /WX)
endif ()

include(AddSelfContainmentTest)

if (EXPECTED_COMPILE_UNIT_TESTS)
  add_self_containment_test(${PROJECT_NAME}.SelfContainmentTest
    TARGET ${PROJECT_NAME}
    HEADERS ${header_files}
  )

  add_subdirectory("test")
endif ()

##############################################################################
# Installation
##############################################################################

if (IS_SUBPROJECT)
  return()
endif ()


include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(EXPECTED_CMAKE_CONFIG_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

# The generated *ConfigVersion is strongly tied to the architecture
# it was generated on, and sets variables such as 'SIZEOF_VOID_P'.
# Since this library is header-only, the host architecture does not
# actually affect the target that consumes this project, so we fake
# the variable to be empty, but reset it after.
#
# Otherwise a 64-bit creator would cause a 32-bit consumer to fail to
# use this library, even though it's header-only.
set(EXPECTED_CMAKE_SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P})
set(CMAKE_SIZEOF_VOID_P "")
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY "SameMajorVersion"
)
set(CMAKE_SIZEOF_VOID_P ${EXPECTED_CMAKE_SIZEOF_VOID_P})

configure_package_config_file(
  "${CMAKE_CURRENT_LIST_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION "${EXPECTED_CMAKE_CONFIG_DESTINATION}"
)

# Targets
install(
  TARGETS "${PROJECT_NAME}"
  EXPORT "${PROJECT_NAME}Targets"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
install(
  EXPORT "${PROJECT_NAME}Targets"
  NAMESPACE "${PROJECT_NAME}::"
  DESTINATION "${EXPECTED_CMAKE_CONFIG_DESTINATION}"
)
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  DESTINATION "${EXPECTED_CMAKE_CONFIG_DESTINATION}"
)

# Includes
install(
  DIRECTORY "include/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
