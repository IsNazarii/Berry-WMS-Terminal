#!/bin/bash
# One file that runs on any recent Linux, Qt inside: Berry_WMS_Terminal-<version>-x86_64.AppImage.
#   packaging/build-appimage.sh [build folder]     (default: build-appimage next to the sources)
# Builds Release with the tests (the AppImage is made only if every test passes), installs into
# an AppDir, lets linuxdeploy's Qt plugin copy in Qt with the QML modules and plugins the program
# loads at run time, and packs it with appimagetool, whose runtime needs no libfuse2.
#
# The AppImage runs on systems with the same glibc as the build machine or newer, so the release
# one is built on the oldest system we support (Ubuntu 24.04, in CI) with Qt from qt.io.
# QMAKE=/path/to/qmake picks a Qt other than the one in PATH.
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
out=${1:-$repo/build-appimage}
appdir=$out/AppDir

# Pinned tool versions, checked by SHA-256: a download that changed is refused, not run.
tools=${XDG_CACHE_HOME:-$HOME/.cache}/berry-wms/appimage-tools
fetch() {  # fetch <file> <url> <sha256>
    local file=$tools/$1
    if [[ ! -f $file ]]; then
        mkdir -p "$tools"
        curl -fsSL --retry 3 -o "$file.part" "$2"
        mv "$file.part" "$file"
    fi
    if ! echo "$3  $file" | sha256sum --check --status; then
        echo "build-appimage.sh: $1 does not match its pinned SHA-256; delete it to download again" >&2
        exit 1
    fi
    chmod +x "$file"
}
fetch linuxdeploy-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage \
    c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
fetch linuxdeploy-plugin-qt-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage \
    15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724
fetch appimagetool-x86_64.AppImage \
    https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage \
    ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0
# The tools are AppImages themselves; extracting instead of mounting needs no FUSE on the build machine.
export APPIMAGE_EXTRACT_AND_RUN=1

# No udev rule: an AppImage cannot install system files (the scale port then needs the dialout group).
cmake -S "$repo" -B "$out" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
      -DWMS_BUILD_TOOLS=OFF -DWMS_INSTALL_UDEV_RULE=OFF
cmake --build "$out"
ctest --test-dir "$out" --output-on-failure
rm -rf "$appdir"
DESTDIR=$appdir cmake --install "$out"

# Qt goes inside under the LGPL, which wants its text and the GPL's to travel with it.
doc=$appdir/usr/share/doc/berry-wms
for license in LGPL-3 GPL-3; do
    if [[ -f /usr/share/common-licenses/$license ]]; then
        cp /usr/share/common-licenses/$license "$doc/$license.txt"
    else
        echo "build-appimage.sh: no /usr/share/common-licenses/$license to put next to the bundled Qt" >&2
        exit 1
    fi
done

version=$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$out/CMakeCache.txt")
app_id=io.github.isnazarii.BerryWms
# QML is compiled into the program, so the plugin scans the sources for imports. The SVG image
# plugin (tab icons) and native Wayland are loaded at run time and nothing links them.
export QML_SOURCES_PATHS=$repo/ui
export EXTRA_QT_MODULES="svg;waylandclient"
export EXTRA_PLATFORM_PLUGINS="libqwayland.so"
# The Qt plugin finds Qt through qmake (Ubuntu names it qmake6) and is itself found in PATH.
export QMAKE=${QMAKE:-$(command -v qmake6 || command -v qmake)}
export PATH=$tools:$PATH
# The Qt plugin brings the Wayland platform but not its client plugins, and without a shell
# integration (xdg-shell) no window opens on Wayland, the default of GNOME on Fedora and Ubuntu.
wayland_plugins=()
for kind in shell-integration decoration-client graphics-integration-client; do
    mkdir -p "$appdir/usr/plugins"
    cp -r "$("$QMAKE" -query QT_INSTALL_PLUGINS)/wayland-$kind" "$appdir/usr/plugins/"
    wayland_plugins+=(--deploy-deps-only "$appdir/usr/plugins/wayland-$kind")
done
"$tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" "${wayland_plugins[@]}" \
    --executable "$appdir/usr/bin/berry-wms" \
    --desktop-file "$appdir/usr/share/applications/$app_id.desktop" \
    --icon-file "$appdir/usr/share/icons/hicolor/scalable/apps/$app_id.svg" \
    --plugin qt

image=$out/Berry_WMS_Terminal-$version-x86_64.AppImage
ARCH=x86_64 "$tools/appimagetool-x86_64.AppImage" --no-appstream "$appdir" "$image"
echo
ls -sh "$image"
