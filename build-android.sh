#!/bin/bash
set -euo pipefail
# Cross-compile an Android arm64 release from Linux and pack it as an APK.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/prereqs.env
source "$ROOT/scripts/prereqs.env"
# shellcheck source=scripts/find-cmake.sh
source "$ROOT/scripts/find-cmake.sh"

export QT_HOST_PATH="$QT_INSTALL_BASE/$QT_VERSION/$QT_HOST_DIRNAME"
QT_ANDROID_PREFIX="$QT_INSTALL_BASE/$QT_VERSION/$QT_ANDROID_DIRNAME"
export ANDROID_SDK_ROOT
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/$ANDROID_NDK_VERSION"
export QT_ANDROID_TOOLCHAIN="$QT_ANDROID_PREFIX/lib/cmake/Qt6/qt.toolchain.cmake"

if [ ! -f "$QT_ANDROID_TOOLCHAIN" ]; then
    echo "Qt Android kit not found at $QT_ANDROID_PREFIX" >&2
    echo "Run ./install-prereqs.sh --mode android" >&2
    exit 1
fi
if [ ! -d "$ANDROID_NDK_ROOT" ]; then
    echo "Android NDK not found at $ANDROID_NDK_ROOT" >&2
    echo "Run ./install-prereqs.sh --mode android" >&2
    exit 1
fi

"$CMAKE" --preset android-release \
    -DCMAKE_PREFIX_PATH="$QT_ANDROID_PREFIX" \
    -DQT_HOST_PATH="$QT_HOST_PATH" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
    -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT" \
    -DANDROID_PLATFORM="$ANDROID_PLATFORM"
"$CMAKE" --build --preset android-release --target apk

mkdir -p "$ROOT/dist"
mapfile -t apks < <(find "$ROOT/build/android-release" -name '*.apk' -type f)
if [ "${#apks[@]}" -eq 0 ]; then
    echo "APK build finished but no .apk was produced" >&2
    exit 1
fi
cp -f "${apks[@]}" "$ROOT/dist/"
echo "APK packages:"
find "$ROOT/dist" -maxdepth 1 -name '*.apk' -print
