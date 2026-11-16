#!/bin/bash
set -euo pipefail
# Cross-compile a Windows release from Linux and pack it as an NSIS installer.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/prereqs.env
source "$ROOT/scripts/prereqs.env"
# shellcheck source=scripts/find-cmake.sh
source "$ROOT/scripts/find-cmake.sh"

export QT_TARGET_PREFIX="$QT_INSTALL_BASE/$QT_VERSION/$QT_WINDOWS_DIRNAME"
export QT_HOST_PATH="$QT_INSTALL_BASE/$QT_VERSION/$QT_HOST_DIRNAME"

if [ ! -f "$QT_TARGET_PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ]; then
    echo "Qt Windows kit not found at $QT_TARGET_PREFIX" >&2
    echo "Run ./install-prereqs.sh --mode windows" >&2
    exit 1
fi

"$CMAKE" --preset windows-mingw-release \
    -DCMAKE_PREFIX_PATH="$QT_TARGET_PREFIX" \
    -DQt6_DIR="$QT_TARGET_PREFIX/lib/cmake/Qt6" \
    -DQT_HOST_PATH="$QT_HOST_PATH" \
    -DQT_TARGET_PREFIX="$QT_TARGET_PREFIX"
"$CMAKE" --build --preset windows-mingw-release --target package

mkdir -p "$ROOT/dist"
mapfile -t packages < <(find "$ROOT/build/windows-mingw-release" -maxdepth 1 -name '*.exe' -type f)
if [ "${#packages[@]}" -eq 0 ]; then
    echo "NSIS build finished but no .exe installer was produced" >&2
    exit 1
fi
cp -f "${packages[@]}" "$ROOT/dist/"
echo "NSIS installers:"
printf '  %s\n' "${packages[@]##*/}"
