#!/bin/bash
set -euo pipefail
# Host test run. Uses the CMake resolved by scripts/find-cmake.sh.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=prereqs.env
source "$ROOT/scripts/prereqs.env"
# shellcheck source=find-cmake.sh
source "$ROOT/scripts/find-cmake.sh"

"$CMAKE" --preset linux-debug
"$CMAKE" --build --preset linux-debug
"$CTEST" --test-dir "$ROOT/build/linux-debug" --output-on-failure
