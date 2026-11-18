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
if [ ! -x "$ANDROID_JAVA_HOME/bin/java" ]; then
    echo "JDK 17 not found at $ANDROID_JAVA_HOME" >&2
    echo "Run ./install-prereqs.sh --mode android" >&2
    exit 1
fi
export JAVA_HOME="$ANDROID_JAVA_HOME"
export PATH="$JAVA_HOME/bin:$PATH"

KEYSTORE="${WATERFALL_KEYSTORE:-$HOME/.android/waterfall-release.keystore}"
KEY_ALIAS="${WATERFALL_KEY_ALIAS:-waterfall}"
APKSIGNER="$ANDROID_SDK_ROOT/build-tools/$ANDROID_BUILD_TOOLS/apksigner"
if [ ! -f "$KEYSTORE" ]; then
    echo "Release keystore not found at $KEYSTORE" >&2
    echo "Create it with keytool, or set WATERFALL_KEYSTORE to its path." >&2
    exit 1
fi
if [ -z "${WATERFALL_KEYSTORE_PASSWORD:-}" ]; then
    echo "Set WATERFALL_KEYSTORE_PASSWORD to the keystore password." >&2
    exit 1
fi
if [ ! -x "$APKSIGNER" ]; then
    echo "apksigner not found at $APKSIGNER" >&2
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

APK="$ROOT/build/android-release/android-build/build/outputs/apk/release/android-build-release-unsigned.apk"
if [ ! -f "$APK" ]; then
    echo "APK build finished but $APK was not produced" >&2
    exit 1
fi
STAMP_CPP="$ROOT/build/android-release/generated/build_timestamp.cpp"
VERSION="$(sed -n 's/^const char \*VERSION="\(.*\)";/\1/p' "$STAMP_CPP")"
if [ -z "$VERSION" ]; then
    echo "Build stamp missing from $STAMP_CPP" >&2
    exit 1
fi
mkdir -p "$ROOT/dist"
OUT="$ROOT/dist/waterfall-${VERSION}-arm64-v8a.apk"
KEY_PASS_ARGS=()
if [ -n "${WATERFALL_KEY_PASSWORD:-}" ]; then
    KEY_PASS_ARGS=(--key-pass env:WATERFALL_KEY_PASSWORD)
fi
"$APKSIGNER" sign \
    --ks "$KEYSTORE" \
    --ks-key-alias "$KEY_ALIAS" \
    --ks-pass env:WATERFALL_KEYSTORE_PASSWORD \
    "${KEY_PASS_ARGS[@]}" \
    --out "$OUT" \
    "$APK"
"$APKSIGNER" verify "$OUT" >/dev/null
rm -f "$ROOT/dist/waterfall-${VERSION}-arm64-v8a-unsigned.apk"
echo "Signed APK:"
echo "  ${OUT##*/}"
