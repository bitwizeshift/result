#!/usr/bin/env bash
################################################################################
# Travis-CI : deploy-doxygen
# --------------------------
#
# Description:
#   Generates doxygen output, and pushes it to the gh-pages branch of the Expcted
#   repository
#
# Requirements:
#   doxygen installed
#   graphviz installed
#   conan installed
#
# Environment:
#   GITHUB_REPO_REF      : Reference to the github repo
#   GITHUB_TOKEN         : Access token for github url
#   TRAVIS_BUILD_NUMBER  : The build number that generates the documentation
#   TRAVIS_COMMIT        : The commit hash that generates the documentatin
################################################################################

set -e

if [[ $# -gt 1 ]]; then
  echo >&2 "Expected at most 1 argument. Received $#."
  echo >&2 "Usage: $0 [latest]"
  exit 1
fi

version="latest"
if [[ $# -eq 0 ]]; then
  version="v$(conan inspect . --attribute version | sed 's@version: @@g')"
elif [[ $1 != "latest" ]]; then
  echo >&2 "Expected at most 1 argument. Received $#."
  echo >&2 "Usage: $0 [latest]"
  exit 1
fi

# Generate documentation
doxygen_output_path="$(pwd)/build/doc"
mkdir -p "${doxygen_output_path}"
doxygen "$(pwd)/.codedocs"

if [[ ! -d "${doxygen_output_path}/html" ]] || [[ ! -f "${doxygen_output_path}/html/index.html" ]]; then
  echo 'Error: No documentation (html) files have been found!' >&2
  exit 1
fi

dist_path="$(pwd)/dist"
api_doc_path="${dist_path}/api/${version}"

# Clone a git repo for doxygen
git clone --single-branch -b gh-pages "https://${GITHUB_REPO_REF}" "${dist_path}"
git config --global push.default simple

# Add a .nojekyll file
touch "dist/.nojekyll"

# Exchange the old api content for the new content
rm -rf "${api_doc_path}"
mkdir -p "${api_doc_path}"
mv ${doxygen_output_path}/html/* "${api_doc_path}"

# Add everything and upload
(
  cd "${dist_path}"
  git add --all
  git commit \
    -m "Deploy codedocs to Github Pages" \
    -m "Documentation updated by build ${TRAVIS_BUILD_NUMBER}." \
    -m "Commit: '${TRAVIS_COMMIT}'" \
    --author "Deployment Bot <deploy@travis-ci.org>" \
    --no-gpg-sign
  git push \
    --force "https://${GITHUB_TOKEN}@${GITHUB_REPO_REF}" gh-pages \
    > /dev/null 2>&1
)
