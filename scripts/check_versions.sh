#!/usr/bin/env bash
# Fail if the version declarations in the repo disagree.
# Source of truth: the newest "## [X.Y.Z]" heading in CHANGELOG.md.
# Run by the versions CI job on every push; run it yourself before a release.
set -euo pipefail
cd "$(dirname "$0")/.."

ver=$(grep -m1 -oP '^## \[\K[0-9]+\.[0-9]+\.[0-9]+' CHANGELOG.md)
fail=0

check() { # <label> <actual>
  if [[ "$2" == "$ver" ]]; then
    echo "  ok    $1 = $2"
  else
    echo "  FAIL  $1 = $2 (expected $ver)"
    fail=1
  fi
}

echo "CHANGELOG.md (source of truth): $ver"
check "CMakeLists.txt  project VERSION" \
  "$(grep -m1 -oP 'VERSION \K[0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt)"
check "CITATION.cff    version" \
  "$(grep -m1 -oP '^version: \K[0-9]+\.[0-9]+\.[0-9]+' CITATION.cff)"
check "mainApp.cpp     APP_VERSION" \
  "$(grep -m1 -oP 'APP_VERSION = "\K[0-9]+\.[0-9]+\.[0-9]+' mainApp.cpp)"

if [[ $fail -ne 0 ]]; then
  echo "Version mismatch -- update every declaration before releasing."
  exit 1
fi
echo "All version declarations agree on $ver."
