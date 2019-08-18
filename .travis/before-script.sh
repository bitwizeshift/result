#!/usr/bin/env bash
################################################################################
# Travis-CI : before-script
# -------------------------
#
# Prints out version details of the various installed utilities. This is
# useful for debugging agent failures.
################################################################################

set -e

# Dump settings
uname -a

if ! test -z "${DOXYGEN_AGENT}"; then
  doxygen --version
else
  for cmd in ${CXX_COMPILER} python3 ninja cmake conan; do
    echo "$cmd --version"
    $cmd --version
  done
fi
