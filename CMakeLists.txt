cmake_minimum_required(VERSION 3.14)

project(
    Result
    VERSION 1.0.0
    DESCRIPTION "A lightweight C++11-compatible error-handling mechanism"
    HOMEPAGE_URL "https://github.com/bitwizeshift/result"
    LANGUAGES CXX
)

string(
    COMPARE EQUAL
    "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}"
    is_top_project
)

# ---- Warning guard ----

# Protect dependents from this project's warnings if the guard isn't disabled
set(Result_warning_guard SYSTEM)
if(is_top_project OR Result_INCLUDE_WITHOUT_SYSTEM)
  set(Result_warning_guard "")
endif()

# ---- Declare library ----

add_library(Result_Result INTERFACE)
add_library(Result::Result ALIAS Result_Result)

set_property(TARGET Result_Result PROPERTY EXPORT_NAME Result)

target_include_directories(
    Result_Result ${Result_warning_guard}
    INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

target_compile_features(Result_Result INTERFACE cxx_std_11)

# ---- Install ----

if(is_top_project)
  set(CMAKE_INSTALL_INCLUDEDIR "include/Result" CACHE PATH "")
endif()

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT Result_Development
)

install(
    TARGETS Result_Result
    EXPORT ResultTargets
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

write_basic_package_version_file(
    ResultConfigVersion.cmake
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT
)

set(
    RESULT_CMAKE_CONFIG_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Result"
    CACHE PATH "CMake package config location relative to the install prefix"
)
mark_as_advanced(RESULT_CMAKE_CONFIG_DESTINATION)

install(
    FILES
    "${PROJECT_SOURCE_DIR}/cmake/ResultConfig.cmake"
    "${PROJECT_BINARY_DIR}/ResultConfigVersion.cmake"
    DESTINATION "${RESULT_CMAKE_CONFIG_DESTINATION}"
    COMPONENT Result_Development
)

install(
    EXPORT ResultTargets
    NAMESPACE Result::
    DESTINATION "${RESULT_CMAKE_CONFIG_DESTINATION}"
    COMPONENT Result_Development
)

if(is_top_project)
  include(CPack)
endif()
