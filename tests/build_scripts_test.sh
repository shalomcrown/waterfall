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

for script in install-prereqs.sh build-linux.sh build-windows.sh build-android.sh install-apk.sh upload-firebase.sh scripts/run-tests.sh; do
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
require_line "$ROOT/install-prereqs.sh" "openjdk-21-jdk"
require_line "$ROOT/install-prereqs.sh" "imagemagick"
# Google Play: Qt 6.11 kit, target SDK 36, 16 KB pages, and an App Bundle on every run.
require_line "$ROOT/scripts/prereqs.env" "QT_ANDROID_VERSION=6.11"
require_line "$ROOT/scripts/prereqs.env" "ANDROID_PLATFORM=android-36"
require_line "$ROOT/scripts/prereqs.env" "ANDROID_NATIVE_PLATFORM=android-35"
require_line "$ROOT/scripts/prereqs.env" "ANDROID_NDK_VERSION=27"
require_line "$ROOT/build-android.sh" "QT_ANDROID_VERSION"
require_line "$ROOT/build-android.sh" "ANDROID_NATIVE_PLATFORM"
require_line "$ROOT/build-android.sh" "--target aab"
require_line "$ROOT/build-android.sh" "jarsigner"
require_line "$ROOT/build-android.sh" "0x4000"
require_line "$ROOT/build-android.sh" "targetSdkVersion:'36'"
require_line "$ROOT/build-android.sh" "sdkVersion:'28'"
require_line "$ROOT/build-android.sh" "com.kirayim.waterfall"
require_line "$ROOT/CMakeLists.txt" "QT_ANDROID_TARGET_SDK_VERSION 36"
require_line "$ROOT/CMakeLists.txt" "QT_ANDROID_MIN_SDK_VERSION 28"
require_line "$ROOT/CMakeLists.txt" 'QT_ANDROID_PACKAGE_NAME "com.kirayim.waterfall"'
require_line "$ROOT/CMakeLists.txt" "max-page-size=16384"
require_line "$ROOT/CMakeLists.txt" "waterfall_make_aab"
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
require_line "$ROOT/install-prereqs.sh" "https://firebase.tools"
require_line "$ROOT/install-prereqs.sh" "ensure_firebase"
require_line "$ROOT/upload-firebase.sh" "appdistribution:distribute"
require_line "$ROOT/upload-firebase.sh" "1:199263152632:android:4db0f5d1770b2da1254e0d"
require_line "$ROOT/upload-firebase.sh" "FIREBASE_APP_ID"
require_line "$ROOT/upload-firebase.sh" "unsigned"
require_line "$ROOT/upload-firebase.sh" "No tester group"
require_line "$ROOT/upload-firebase.sh" "appdistribution:testers:list"
require_line "$ROOT/upload-firebase.sh" "GROUP_ALIASES"
if grep -E -q '(^|[[:space:]])GROUPS=' "$ROOT/upload-firebase.sh"; then
    echo "upload-firebase.sh must not assign GROUPS; bash uses it for numeric group ids" >&2
    exit 1
fi
if grep -E -q '^(export )?FIREBASE_TOKEN=' "$ROOT/upload-firebase.sh" "$ROOT/scripts/prereqs.env" "$ROOT/install-prereqs.sh"; then
    echo "Firebase token must come from the environment, not the repo" >&2
    exit 1
fi
require_line "$ROOT/build-android.sh" 'export JAVA_HOME="$ANDROID_JAVA_HOME"'
require_line "$ROOT/build-android.sh" "apksigner"
require_line "$ROOT/build-android.sh" "env:WATERFALL_KEYSTORE_PASSWORD"
require_line "$ROOT/build-android.sh" "--full"
require_line "$ROOT/build-android.sh" "WATERFALL_FULL_UNLOCK"
require_line "$ROOT/build-android.sh" '-full'
require_line "$ROOT/CMakeLists.txt" "WATERFALL_FULL_UNLOCK"
require_line "$ROOT/upload-firebase.sh" "Refusing to upload a locked APK"
require_line "$ROOT/upload-firebase.sh" "full-arm64-v8a.apk"
if grep -E -q '^(export )?WATERFALL_(KEYSTORE|KEY)_PASSWORD=' "$ROOT/build-android.sh" "$ROOT/scripts/prereqs.env"; then
    echo "keystore password must come from the environment, not the repo" >&2
    exit 1
fi
require_line "$ROOT/scripts/prereqs.env" "ANDROID_JAVA_HOME=.*java-21"

require_line "$ROOT/resources/waterfall.desktop.in" "Icon=waterfall"
require_line "$ROOT/resources/waterfall.desktop.in" "StartupWMClass=waterfall"
require_line "$ROOT/resources/waterfall.svg" "<svg"
require_line "$ROOT/CMakeLists.txt" "share/icons/hicolor/scalable/apps"
require_line "$ROOT/CMakeLists.txt" 'set(CPACK_GENERATOR "NSIS")'
require_line "$ROOT/CMakeLists.txt" 'set(CPACK_GENERATOR "DEB")'
require_line "$ROOT/CMakeLists.txt" "if(ANDROID)"
require_line "$ROOT/CMakeLists.txt" "render-android-icons.cmake"
require_line "$ROOT/android/AndroidManifest.xml" "android:icon=\"@mipmap/ic_launcher\""
require_line "$ROOT/android/res/mipmap-anydpi-v26/ic_launcher.xml" "adaptive-icon"

if grep -q 'windows-mingw' "$ROOT/build-linux.sh"; then
    echo "build-linux.sh must not use the Windows preset" >&2
    exit 1
fi
if grep -q 'android-release' "$ROOT/build-linux.sh"; then
    echo "build-linux.sh must not use the Android preset" >&2
    exit 1
fi

# An empty tester list, and the CLI error payload with no "result" key, both upload
# without sharing. A fake firebase CLI stands in for the network.
fake_bin="$(mktemp -d)"
trap 'rm -rf "$fake_bin"' EXIT
fake_apk="$fake_bin/waterfall-test-full-arm64-v8a.apk"
locked_apk="$fake_bin/waterfall-test-arm64-v8a.apk"
touch "$fake_apk" "$locked_apk"
cat >"$fake_bin/firebase" <<'EOF'
#!/bin/bash
if [[ "$*" == *projects:list* ]]; then
    printf '%s\n' '{"status":"success","result":[{"projectId":"waterfall-b5c4a","projectNumber":"199263152632"}]}'
    exit 0
fi
if [[ "$*" == *testers:list* ]]; then
    if [ "${FAKE_TESTERS_MODE:-error}" = "empty" ]; then
        printf '%s\n' '{"status":"success","result":{"testers":[]}}'
        exit 0
    fi
    printf '%s\n' '{"status":"error","error":"Failed to list testers."}'
    exit 1
fi
if [[ "$*" == *appdistribution:distribute* ]]; then
    printf '%s\n' "distributed $*"
    exit 0
fi
echo "unexpected firebase args: $*" >&2
exit 1
EOF
chmod +x "$fake_bin/firebase"
run_upload() {
    PATH="$fake_bin:$PATH" "$ROOT/upload-firebase.sh" "$fake_apk"
}
if PATH="$fake_bin:$PATH" "$ROOT/upload-firebase.sh" "$locked_apk"; then
    echo "a locked APK must be refused" >&2
    exit 1
fi
error_output="$(FAKE_TESTERS_MODE=error run_upload)"
printf '%s\n' "$error_output" | grep -q "No testers in this project. The build will be uploaded and not shared."
printf '%s\n' "$error_output" | grep -q "distributed"
printf '%s\n' "$error_output" | grep -q "Firebase project: waterfall-b5c4a"
empty_output="$(FAKE_TESTERS_MODE=empty run_upload)"
printf '%s\n' "$empty_output" | grep -q "No testers in this project. The build will be uploaded and not shared."
printf '%s\n' "$empty_output" | grep -q "distributed"
if printf '%s\n' "$error_output" "$empty_output" | grep -q -- '--testers'; then
    echo "upload with no testers must not pass --testers" >&2
    exit 1
fi

echo "build_scripts_test: ok"
