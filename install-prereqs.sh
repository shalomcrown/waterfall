#!/bin/bash
set -euo pipefail
#
# Install prerequisites for waterfall.
# Edit scripts/prereqs.env and the package lists below when prerequisites change,
# then re-run this script. The build scripts read the same pins.
#
# Usage:
#   ./install-prereqs.sh [options]
#   --mode <all|linux|windows|android>   (default: all)
#   --dry-run

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/prereqs.env
source "$ROOT/scripts/prereqs.env"

MODE="all"
DRY_RUN=0
MISSING_COUNT=0

LINUX_PACKAGES=(
    ninja-build
    g++
    pkg-config
    qt6-base-dev
    qt6-multimedia-dev
    dpkg-dev
)
WINDOWS_PACKAGES=(
    ninja-build
    pkg-config
    mingw-w64
    nsis
    unzip
    wget
    python3
    python3-pip
)
ANDROID_PACKAGES=(
    ninja-build
    unzip
    wget
    python3
    python3-pip
    openjdk-17-jdk
)

if [ -t 1 ]; then
    C_RED='\033[0;31m'; C_GREEN='\033[0;32m'
    C_YELLOW='\033[1;33m'; C_BLUE='\033[0;34m'; C_RESET='\033[0m'
else
    C_RED=''; C_GREEN=''; C_YELLOW=''; C_BLUE=''; C_RESET=''
fi
info()  { printf '%b[INFO]%b  %s\n' "$C_BLUE"   "$C_RESET" "$1"; }
ok()    { printf '%b[OK]%b    %s\n' "$C_GREEN"  "$C_RESET" "$1"; }
step()  { printf '%b[STEP]%b  %s\n' "$C_YELLOW" "$C_RESET" "$1"; }
fail()  { MISSING_COUNT=$((MISSING_COUNT + 1)); printf '%b[FAIL]%b  %s\n' "$C_RED" "$C_RESET" "$1"; }
die()   { printf '%b[ERROR]%b %s\n' "$C_RED" "$C_RESET" "$1" >&2; exit 1; }
run() {
    if [ "$DRY_RUN" -eq 1 ]; then
        printf '%b[DRY-RUN]%b  %s\n' "$C_YELLOW" "$C_RESET" "$*"
        return 0
    fi
    "$@"
}

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    command -v sudo >/dev/null 2>&1 && SUDO="sudo" || die "sudo is required for package installation"
fi

while [ "$#" -gt 0 ]; do
    case "$1" in
        --mode) MODE="$2"; shift 2 ;;
        --dry-run) DRY_RUN=1; shift ;;
        -h|--help)
            grep '^#' "$0" | grep -v '^#!' | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *) die "Unknown argument: $1" ;;
    esac
done
case "$MODE" in
    all|linux|windows|android) ;;
    *) die "Invalid --mode '$MODE'" ;;
esac

apt_install() {
    step "apt-get install $*"
    run $SUDO apt-get install -y "$@"
}

cmake_version_ok() {
    local bin="$1" ver
    [ -n "$bin" ] && [ -x "$bin" ] || return 1
    ver="$("$bin" --version 2>/dev/null | awk 'NR==1 { print $3 }')" || return 1
    [ -n "$ver" ] || return 1
    [ "$(printf '%s\n%s\n' "$CMAKE_MIN_VERSION" "$ver" | sort -V | head -n 1)" = "$CMAKE_MIN_VERSION" ]
}

ensure_cmake() {
    local candidate
    for candidate in /usr/local/bin/cmake /opt/cmake/bin/cmake "$(command -v cmake 2>/dev/null || true)"; do
        if cmake_version_ok "$candidate"; then
            ok "cmake $($candidate --version | awk 'NR==1 { print $3 }')"
            return 0
        fi
    done
    step "Install CMake ${CMAKE_VERSION} (apt cmake is older than ${CMAKE_MIN_VERSION})"
    if [ "$DRY_RUN" -eq 1 ]; then
        info "Would install CMake ${CMAKE_VERSION} into /opt/cmake"
        return 0
    fi
    local tmp url
    tmp="$(mktemp -d)"
    url="https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz"
    wget -O "$tmp/cmake.tar.gz" "$url"
    tar -xzf "$tmp/cmake.tar.gz" -C "$tmp"
    run $SUDO rm -rf /opt/cmake
    run $SUDO mv "$tmp/cmake-${CMAKE_VERSION}-linux-x86_64" /opt/cmake
    run $SUDO ln -sfn /opt/cmake/bin/cmake /usr/local/bin/cmake
    run $SUDO ln -sfn /opt/cmake/bin/ctest /usr/local/bin/ctest
    run $SUDO ln -sfn /opt/cmake/bin/cpack /usr/local/bin/cpack
    rm -rf "$tmp"
    ok "cmake ${CMAKE_VERSION}"
}

ensure_aqt() {
    if python3 -m aqt version >/dev/null 2>&1; then
        ok "aqtinstall $(python3 -m aqt version 2>/dev/null | awk 'NR==1 { print $NF }')"
        return 0
    fi
    step "pip install aqtinstall==${AQTINSTALL_VERSION}"
    if [ "$DRY_RUN" -eq 1 ]; then
        info "Would pip install aqtinstall==${AQTINSTALL_VERSION}"
        return 0
    fi
    if ! python3 -m pip install --user "aqtinstall==${AQTINSTALL_VERSION}"; then
        python3 -m pip install --user --break-system-packages "aqtinstall==${AQTINSTALL_VERSION}"
    fi
    ok "aqtinstall ${AQTINSTALL_VERSION}"
}

qt_prefix() {
    printf '%s/%s/%s' "$QT_INSTALL_BASE" "$QT_VERSION" "$1"
}

ensure_qt_arch() {
    local host="$1" target="$2" arch="$3" dirname="$4"
    local prefix
    prefix="$(qt_prefix "$dirname")"
    if [ -f "$prefix/lib/cmake/Qt6/Qt6Config.cmake" ]; then
        ok "Qt ${QT_VERSION} ${dirname}"
        ensure_qt_module "$host" "$target" "$arch" "$dirname" "Multimedia" "qtmultimedia"
        return 0
    fi
    step "aqt install-qt ${host} ${target} ${QT_VERSION} ${arch}"
    run $SUDO mkdir -p "$QT_INSTALL_BASE"
    if [ "$DRY_RUN" -eq 0 ]; then
        $SUDO chown "$USER:$USER" "$QT_INSTALL_BASE" 2>/dev/null || true
    fi
    run python3 -m aqt install-qt -b "$AQT_MIRROR" -O "$QT_INSTALL_BASE" "$host" "$target" "$QT_VERSION" "$arch"
    if [ "$DRY_RUN" -eq 0 ] && [ ! -f "$prefix/lib/cmake/Qt6/Qt6Config.cmake" ]; then
        fail "Qt ${dirname} did not appear at $prefix"
    fi
    ensure_qt_module "$host" "$target" "$arch" "$dirname" "Multimedia" "qtmultimedia"
}

ensure_qt_module() {
    local host="$1" target="$2" arch="$3" dirname="$4" cmake_name="$5" aqt_name="$6"
    local prefix cmake_file
    prefix="$(qt_prefix "$dirname")"
    cmake_file="$prefix/lib/cmake/Qt6${cmake_name}/Qt6${cmake_name}Config.cmake"
    if [ -f "$cmake_file" ]; then
        ok "Qt ${cmake_name} ($dirname)"
        return 0
    fi
    step "aqt install Qt ${cmake_name} for ${dirname}"
    run python3 -m aqt install-qt -b "$AQT_MIRROR" -O "$QT_INSTALL_BASE" \
        "$host" "$target" "$QT_VERSION" "$arch" -m "$aqt_name"
    if [ "$DRY_RUN" -eq 0 ] && [ ! -f "$cmake_file" ]; then
        fail "Qt ${cmake_name} did not appear at $cmake_file"
    fi
}

ensure_android_udev() {
    local rule="/etc/udev/rules.d/51-android.rules"
    if [ -f "$rule" ] && grep -q '18d1' "$rule"; then
        ok "Android udev rule"
        return 0
    fi
    step "udev rule so adb can open a Pixel over USB"
    if [ "$DRY_RUN" -eq 1 ]; then
        info "Would write $rule and reload udev"
        return 0
    fi
    printf '%s\n' 'SUBSYSTEM=="usb", ATTR{idVendor}=="18d1", MODE="0660", GROUP="plugdev"' \
        | $SUDO tee "$rule" >/dev/null
    $SUDO udevadm control --reload-rules
    $SUDO udevadm trigger
    ok "Android udev rule (unplug and replug the phone)"
}

ensure_android_sdk() {
    local sdkmanager="$ANDROID_SDK_ROOT/cmdline-tools/latest/bin/sdkmanager"
    if [ ! -x "$sdkmanager" ]; then
        step "Install Android command-line tools"
        if [ "$DRY_RUN" -eq 1 ]; then
            info "Would download $ANDROID_CMDLINE_TOOLS_URL"
        else
            local tmp
            tmp="$(mktemp -d)"
            wget -O "$tmp/cmdtools.zip" "$ANDROID_CMDLINE_TOOLS_URL"
            unzip -q "$tmp/cmdtools.zip" -d "$tmp"
            run $SUDO mkdir -p "$ANDROID_SDK_ROOT/cmdline-tools/latest"
            run $SUDO rm -rf "$ANDROID_SDK_ROOT/cmdline-tools/latest"
            run $SUDO mkdir -p "$ANDROID_SDK_ROOT/cmdline-tools/latest"
            run $SUDO cp -a "$tmp/cmdline-tools/." "$ANDROID_SDK_ROOT/cmdline-tools/latest/"
            rm -rf "$tmp"
            if [ "$(id -u)" -ne 0 ]; then
                $SUDO chown -R "$USER:$USER" "$ANDROID_SDK_ROOT" 2>/dev/null || true
            fi
        fi
    else
        ok "Android cmdline-tools"
    fi

    local ndk="$ANDROID_SDK_ROOT/ndk/$ANDROID_NDK_VERSION"
    if [ -d "$ndk" ] && [ -d "$ANDROID_SDK_ROOT/platforms/$ANDROID_PLATFORM" ]; then
        ok "Android SDK platform $ANDROID_PLATFORM and NDK $ANDROID_NDK_VERSION"
        return 0
    fi
    step "sdkmanager platforms, build-tools, ndk"
    if [ "$DRY_RUN" -eq 1 ]; then
        info "Would install platforms;$ANDROID_PLATFORM build-tools;$ANDROID_BUILD_TOOLS ndk;$ANDROID_NDK_VERSION"
        return 0
    fi
    set +o pipefail
    yes | "$sdkmanager" --sdk_root="$ANDROID_SDK_ROOT" --licenses >/dev/null || true
    set -o pipefail
    "$sdkmanager" --sdk_root="$ANDROID_SDK_ROOT" \
        "platform-tools" \
        "platforms;$ANDROID_PLATFORM" \
        "build-tools;$ANDROID_BUILD_TOOLS" \
        "ndk;$ANDROID_NDK_VERSION"
    ok "Android SDK"
}

echo
info "Mode: $MODE"
info "Qt for Windows/Android: $QT_VERSION"
echo

if [ "$MODE" = "all" ] || [ "$MODE" = "linux" ]; then
    info "────── Linux native prerequisites ──────"
    apt_install "${LINUX_PACKAGES[@]}"
    ensure_cmake
    echo
fi

if [ "$MODE" = "all" ] || [ "$MODE" = "windows" ]; then
    info "────── Windows cross-compilation prerequisites ──────"
    apt_install "${WINDOWS_PACKAGES[@]}"
    ensure_cmake
    ensure_aqt
    ensure_qt_arch linux desktop "$QT_HOST_ARCH" "$QT_HOST_DIRNAME"
    ensure_qt_arch windows desktop "$QT_WINDOWS_ARCH" "$QT_WINDOWS_DIRNAME"
    echo
fi

if [ "$MODE" = "all" ] || [ "$MODE" = "android" ]; then
    info "────── Android prerequisites ──────"
    apt_install "${ANDROID_PACKAGES[@]}"
    ensure_cmake
    ensure_aqt
    ensure_qt_arch linux desktop "$QT_HOST_ARCH" "$QT_HOST_DIRNAME"
    ensure_qt_arch linux android "$QT_ANDROID_ARCH" "$QT_ANDROID_DIRNAME"
    ensure_android_udev
    ensure_android_sdk
    echo
fi

if [ "$MISSING_COUNT" -eq 0 ]; then
    printf '%bAll prerequisites satisfied.%b\n' "$C_GREEN" "$C_RESET"
    exit 0
fi
printf '%b%d prerequisite(s) missing.%b\n' "$C_RED" "$MISSING_COUNT" "$C_RESET"
exit 1
