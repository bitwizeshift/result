#!/usr/bin/env bash
################################################################################
# Travis-CI : build
# -----------------
#
# Generates a CMake project, builds it, and tests it
################################################################################

set -e

if ! test -z "${CXX_COMPILER}"; then
  export CXX=${CXX_COMPILER}
fi

build_directory="build"

mkdir -p "${build_directory}"
cd "${build_directory}"
conan install ..

cmake .. \
  -G"Ninja" \
  -DEXPECTED_COMPILE_UNIT_TESTS=On \
  ${CMAKE_OPTIONS}

cmake --build . --config Release

# TODO(bitwizeshift):
#   Optimize CI times by syncing the built binaries to a back-end, and running
#   tests as a distinct, separate step. This will allow for a quick-fail if
#   the code fails to compile.
cmake --build . --target test
