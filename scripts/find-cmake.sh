# Resolve a CMake binary that satisfies CMAKE_MIN_VERSION from scripts/prereqs.env.
# Source this file; it sets CMAKE.

if [ -z "${CMAKE_MIN_VERSION:-}" ]; then
    echo "scripts/find-cmake.sh requires scripts/prereqs.env" >&2
    exit 1
fi

cmake_version_ok() {
    local bin="$1" ver
    [ -n "$bin" ] && [ -x "$bin" ] || return 1
    ver="$("$bin" --version 2>/dev/null | awk 'NR==1 { print $3 }')"
    [ -n "$ver" ] || return 1
    [ "$(printf '%s\n%s\n' "$CMAKE_MIN_VERSION" "$ver" | sort -V | head -n 1)" = "$CMAKE_MIN_VERSION" ]
}

_requested_cmake="${CMAKE:-}"
CMAKE=""
if [ -n "$_requested_cmake" ] && cmake_version_ok "$_requested_cmake"; then
    CMAKE="$_requested_cmake"
else
    _candidate=""
    for _candidate in \
        /usr/local/bin/cmake \
        /opt/cmake/bin/cmake \
        /opt/jetbrains/clion/bin/cmake/linux/x64/bin/cmake
    do
        if cmake_version_ok "$_candidate"; then
            CMAKE="$_candidate"
            break
        fi
    done
    if [ -z "$CMAKE" ] && command -v cmake >/dev/null 2>&1; then
        _candidate="$(command -v cmake)"
        if cmake_version_ok "$_candidate"; then
            CMAKE="$_candidate"
        fi
    fi
fi
unset _requested_cmake _candidate

if [ -z "$CMAKE" ]; then
    echo "CMake ${CMAKE_MIN_VERSION} or newer is required. Run ./install-prereqs.sh" >&2
    exit 1
fi

CTEST="$(dirname "$CMAKE")/ctest"
if [ ! -x "$CTEST" ]; then
    CTEST="ctest"
fi
