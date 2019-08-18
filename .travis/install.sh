#!/usr/bin/env bash
################################################################################
# Travis-CI : install
# -------------------
#
# Installs the required tools for the CI build
################################################################################

set -e

# Nothing to install when we do a doxygen build
if ! test -z "${DOXYGEN_AGENT}"; then
  exit 0
fi

if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then
  sudo python3 -m pip install --upgrade --force setuptools
  sudo python3 -m pip install --upgrade --force pip
  python3 -m pip install --user conan==1.11.1
elif [[ "$TRAVIS_OS_NAME" == "osx" ]]; then
  brew install python3 ninja cmake
  python3 -m pip install conan==1.11.1
else
  echo >&2 "Unknown TRAVIS_OS_NAME: '${TRAVIS_OS_NAME}'"
  exit 1
fi