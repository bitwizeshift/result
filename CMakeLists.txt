cmake_minimum_required(VERSION 3.5)

if (PROJECT_NAME)
  set(IS_SUBPROJECT TRUE)
endif ()

set(EXPECTED_CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
set(EXPECTED_CMAKE_TEMPLATE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/templates")

set(CMAKE_MODULE_PATH "${EXPECTED_CMAKE_MODULE_PATH}" "${CMAKE_MODULE_PATH}")

option(EXPECTED_COMPILE_UNIT_TESTS "Compile and run the unit tests for this library" ON)

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
  include/expected.hpp
)

add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME}
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
  INTERFACE $<INSTALL_INTERFACE:include>
)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" AND
    "${CMAKE_CXX_SIMULATE_ID}" STREQUAL "MSVC")
  add_compile_options(-Wall -Werror -pedantic)
elseif( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC" )
  # TODO: Determine MSVC necessary compiler flags
endif ()

include(AddSelfContainmentTest)

if (EXPECTED_COMPILE_UNIT_TESTS)
  add_self_containment_test(${PROJECT_NAME}.SelfContainmentTest
    TARGET ${PROJECT_NAME}
    HEADERS ${header_files}
  )
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
