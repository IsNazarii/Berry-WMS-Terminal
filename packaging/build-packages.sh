#!/bin/bash
# One command for a package of the system it runs on: .deb on Ubuntu/Debian, .rpm on Fedora.
#   packaging/build-packages.sh [build folder]     (default: build-release next to the sources)
# Builds Release with the tests and packages only if every test passes. The package takes Qt
# from the system, so it installs on the same distribution release (or newer) as the build.
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
out=${1:-$repo/build-release}

if command -v dpkg-shlibdeps >/dev/null; then
    generator=DEB
elif command -v rpmbuild >/dev/null; then
    generator=RPM
else
    echo "build-packages.sh: no packaging tools: install dpkg-dev (Ubuntu) or rpm-build (Fedora)" >&2
    exit 1
fi

cmake -S "$repo" -B "$out" -G Ninja -DCMAKE_BUILD_TYPE=Release -DWMS_BUILD_TOOLS=OFF
cmake --build "$out"
ctest --test-dir "$out" --output-on-failure
(cd "$out" && cpack -G "$generator")
echo
find "$out" -maxdepth 1 -name "*.deb" -o -maxdepth 1 -name "*.rpm"
