#!/usr/bin/env bash
# Regression: version.sh writes BASE.YYYYMMDD.HHMM.<git> into C++ and CPack snippets.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# ================================================================================

test_version_sh_writes_stamp_files() {
    bash "$ROOT/version.sh" "$TMP/build_timestamp.cpp" "$TMP/timestamp.cmake"
    if grep -Eq '^const char \*VERSION="0\.1\.0\.[0-9]{8}\.[0-9]{4}\.([0-9a-f]+|unknown)";$' \
            "$TMP/build_timestamp.cpp"; then
        :
    else
        echo "unexpected VERSION in $TMP/build_timestamp.cpp:" >&2
        cat "$TMP/build_timestamp.cpp" >&2
        exit 1
    fi
    if grep -q 'set(CPACK_PACKAGE_VERSION "' "$TMP/timestamp.cmake"; then
        :
    else
        echo "missing CPACK_PACKAGE_VERSION in timestamp.cmake" >&2
        exit 1
    fi
    if grep -q 'waterfall-' "$TMP/timestamp.cmake" && grep -q -- '-win64' "$TMP/timestamp.cmake"; then
        :
    else
        echo "NSIS file name should include waterfall-<version>-win64" >&2
        cat "$TMP/timestamp.cmake" >&2
        exit 1
    fi
}

test_version_sh_writes_stamp_files

# ================================================================================

test_android_manifest_uses_the_build_stamp() {
    printf '%s\n' 'const char *VERSION="0.1.0.20261118.1642.abc1234";' >"$TMP/build_timestamp.cpp"
    cat >"$TMP/AndroidManifest.xml" <<'EOF'
<manifest android:versionCode="1" android:versionName="1.0">
</manifest>
EOF
    bash "$ROOT/version.sh" --android-manifest "$TMP/build_timestamp.cpp" "$TMP/AndroidManifest.xml"
    if grep -q 'android:versionName="0.1.0.20261118.1642.abc1234"' "$TMP/AndroidManifest.xml"; then
        :
    else
        echo "Android versionName was not stamped" >&2
        cat "$TMP/AndroidManifest.xml" >&2
        exit 1
    fi
    if grep -Eq 'android:versionCode="[0-9]{10}"' "$TMP/AndroidManifest.xml"; then
        :
    else
        echo "Android versionCode was not stamped" >&2
        cat "$TMP/AndroidManifest.xml" >&2
        exit 1
    fi
}

test_android_manifest_uses_the_build_stamp
echo "version_script_test: ok"
