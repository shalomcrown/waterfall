#!/bin/bash
# The packaging scripts stay wired to the prerequisite pins and the CPack generators.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

require_line() {
    local file="$1" pattern="$2"
    if ! grep -q -- "$pattern" "$file"; then
        echo "missing '$pattern' in $file" >&2
        exit 1
    fi
}

for script in install-prereqs.sh build-linux.sh build-windows.sh build-android.sh install-apk.sh scripts/run-tests.sh; do
    head -n 2 "$ROOT/$script" | grep -q 'set -euo pipefail' || {
        echo "$script must enable set -euo pipefail" >&2
        exit 1
    }
done

require_line "$ROOT/install-prereqs.sh" "scripts/prereqs.env"
require_line "$ROOT/install-prereqs.sh" "qt6-base-dev"
require_line "$ROOT/install-prereqs.sh" "qt6-multimedia-dev"
require_line "$ROOT/install-prereqs.sh" "dpkg-dev"
require_line "$ROOT/install-prereqs.sh" "mingw-w64"
require_line "$ROOT/install-prereqs.sh" "nsis"
require_line "$ROOT/install-prereqs.sh" "aqtinstall"
require_line "$ROOT/install-prereqs.sh" "openjdk-17-jdk"
require_line "$ROOT/install-prereqs.sh" "ANDROID_NDK_VERSION"
require_line "$ROOT/scripts/prereqs.env" "QT_VERSION="
require_line "$ROOT/scripts/prereqs.env" "ANDROID_NDK_VERSION="

require_line "$ROOT/build-linux.sh" "scripts/prereqs.env"
require_line "$ROOT/build-linux.sh" "--preset linux-release"
require_line "$ROOT/build-linux.sh" "--target package"
require_line "$ROOT/build-windows.sh" "--preset windows-mingw-release"
require_line "$ROOT/build-windows.sh" "--target package"
require_line "$ROOT/build-android.sh" "--preset android-release"
require_line "$ROOT/build-android.sh" "--target apk"
require_line "$ROOT/install-apk.sh" "scripts/prereqs.env"
require_line "$ROOT/install-apk.sh" "adb"
require_line "$ROOT/install-apk.sh" "install -r"
require_line "$ROOT/install-apk.sh" "cannot open the USB device"
require_line "$ROOT/install-prereqs.sh" "51-android.rules"
require_line "$ROOT/build-android.sh" 'export JAVA_HOME="$ANDROID_JAVA_HOME"'
require_line "$ROOT/build-android.sh" "apksigner"
require_line "$ROOT/build-android.sh" "env:WATERFALL_KEYSTORE_PASSWORD"
if grep -E -q '^(export )?WATERFALL_(KEYSTORE|KEY)_PASSWORD=' "$ROOT/build-android.sh" "$ROOT/scripts/prereqs.env"; then
    echo "keystore password must come from the environment, not the repo" >&2
    exit 1
fi
require_line "$ROOT/scripts/prereqs.env" "ANDROID_JAVA_HOME=.*java-17"

require_line "$ROOT/CMakeLists.txt" 'set(CPACK_GENERATOR "NSIS")'
require_line "$ROOT/CMakeLists.txt" 'set(CPACK_GENERATOR "DEB")'
require_line "$ROOT/CMakeLists.txt" "if(ANDROID)"

if grep -q 'windows-mingw' "$ROOT/build-linux.sh"; then
    echo "build-linux.sh must not use the Windows preset" >&2
    exit 1
fi
if grep -q 'android-release' "$ROOT/build-linux.sh"; then
    echo "build-linux.sh must not use the Android preset" >&2
    exit 1
fi

echo "build_scripts_test: ok"
