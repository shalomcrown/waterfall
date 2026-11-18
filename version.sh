#!/bin/bash
# Build stamp: BASE.YYYYMMDD.HHMM.<git-short>
set +a
set +x
set +v

if [ "${1:-}" = "--android-manifest" ]; then
    VERSION="$(sed -n 's/^const char \*VERSION="\(.*\)";/\1/p' "$2")"
    CODE="$(date +%s)"
    if [ -z "$VERSION" ]; then
        echo "Build stamp missing from $2" >&2
        exit 1
    fi
    sed -i \
        -e "s/android:versionCode=\"[^\"]*\"/android:versionCode=\"${CODE}\"/" \
        -e "s/android:versionName=\"[^\"]*\"/android:versionName=\"${VERSION}\"/" \
        "$3"
    exit 0
fi

BASE_VERSION="${WATERFALL_BASE_VERSION:-0.1.0}"
GIT_SHORT="$(git rev-parse --short HEAD 2>/dev/null || echo unknown)"
VERSION="${BASE_VERSION}.$(date +"%Y%m%d.%H%M.")${GIT_SHORT}"

mkdir -p "$(dirname "$1")"
mkdir -p "$(dirname "$2")"

rm -f "$(dirname "$1")"/*.deb "$(dirname "$1")"/*-win64.exe
rm -f "$(dirname "$2")"/*.deb "$(dirname "$2")"/*-win64.exe

echo "$(pwd)"
echo "${VERSION}" "->" "$1" "$2"
cat <<FILE >"$1"
const char *VERSION="${VERSION}";

FILE

cat <<FILE >"$2"
set(CPACK_PACKAGE_VERSION "${VERSION}")

if(CPACK_GENERATOR STREQUAL "DEB")
    set(CPACK_DEBIAN_PACKAGE_VERSION "${VERSION}")
    set(CPACK_DEBIAN_PACKAGE_RELEASE "1")
    set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
elseif(CPACK_GENERATOR STREQUAL "NSIS")
    set(CPACK_PACKAGE_FILE_NAME "waterfall-${VERSION}-win64")
endif()

FILE
