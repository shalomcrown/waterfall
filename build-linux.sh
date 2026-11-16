#!/bin/bash
set -euo pipefail
# Build the Linux release and pack it as a DEB.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/prereqs.env
source "$ROOT/scripts/prereqs.env"
# shellcheck source=scripts/find-cmake.sh
source "$ROOT/scripts/find-cmake.sh"

"$CMAKE" --preset linux-release
"$CMAKE" --build --preset linux-release
"$CTEST" --test-dir "$ROOT/build/linux-release" --output-on-failure
"$CMAKE" --build --preset linux-release --target package

mkdir -p "$ROOT/dist"
mapfile -t packages < <(find "$ROOT/build/linux-release" -maxdepth 1 -name '*.deb' -type f)
if [ "${#packages[@]}" -eq 0 ]; then
    echo "DEB build finished but no .deb was produced" >&2
    exit 1
fi
cp -f "${packages[@]}" "$ROOT/dist/"
echo "DEB packages:"
printf '  %s\n' "${packages[@]##*/}"
