#!/bin/bash
set -euo pipefail
# Install the signed Android APK onto a phone connected over USB.
# Usage: ./install-apk.sh [path-to.apk]
# With no argument, installs the newest signed APK in dist/.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/prereqs.env
source "$ROOT/scripts/prereqs.env"

ADB="${ANDROID_SDK_ROOT}/platform-tools/adb"
if [ ! -x "$ADB" ]; then
    echo "adb not found at $ADB" >&2
    echo "Run ./install-prereqs.sh --mode android" >&2
    exit 1
fi

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
    echo "Usage: ./install-apk.sh [path-to.apk]"
    echo "USB debugging must be on, and the phone must accept the debugging prompt."
    exit 0
fi

if [ -n "${1:-}" ]; then
    APK="$1"
else
    mapfile -t apks < <(find "$ROOT/dist" -maxdepth 1 -type f -name 'waterfall-*-arm64-v8a.apk' ! -name '*-unsigned.apk' -printf '%T@ %p\n' | sort -nr | cut -d' ' -f2-)
    if [ "${#apks[@]}" -eq 0 ]; then
        echo "No signed APK in dist/. Run ./build-android.sh first, or pass a path." >&2
        exit 1
    fi
    APK="${apks[0]}"
fi
if [ ! -f "$APK" ]; then
    echo "APK not found: $APK" >&2
    exit 1
fi

mapfile -t ready < <("$ADB" devices | awk 'NR>1 && $2=="device" { print $1 }')
mapfile -t blocked < <("$ADB" devices | awk 'NR>1 && $2=="no" { print $1 }')
if [ "${#blocked[@]}" -gt 0 ]; then
    echo "Phone ${blocked[0]} is connected, but adb cannot open the USB device." >&2
    echo "Run ./install-prereqs.sh --mode android, then unplug the phone and plug it back in." >&2
    exit 1
fi
mapfile -t pending < <("$ADB" devices | awk 'NR>1 && $2=="unauthorized" { print $1 }')
if [ "${#pending[@]}" -gt 0 ]; then
    echo "Phone ${pending[0]} has not accepted USB debugging." >&2
    echo "Unlock the phone and tap Allow on the debugging prompt." >&2
    exit 1
fi
if [ "${#ready[@]}" -eq 0 ]; then
    echo "No phone in USB debugging mode." >&2
    echo "Enable Settings → System → Developer options → USB debugging, then reconnect." >&2
    exit 1
fi
if [ "${#ready[@]}" -gt 1 ] && [ -z "${ANDROID_SERIAL:-}" ]; then
    echo "More than one device is connected. Set ANDROID_SERIAL to one of:" >&2
    printf '  %s\n' "${ready[@]}" >&2
    exit 1
fi

echo "Installing ${APK##*/}"
"$ADB" install -r "$APK"
