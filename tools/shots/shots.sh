#!/bin/bash
# Screenshots of every screen into <dir>, in a clean environment: a database in memory
# (demo data), scale 100%, and settings in a temporary folder, so the operator's own
# QSettings are neither read nor changed. Run from the repository root after a build:
#   tools/shots/shots.sh /tmp/before
#   ... change the UI, rebuild ...
#   tools/shots/shots.sh /tmp/after && python3 tools/shots/compare.py /tmp/before /tmp/after
set -euo pipefail
dir=${1:?usage: shots.sh <dir>}
config=$(mktemp -d)
trap 'rm -rf "$config"' EXIT
rm -rf "$dir"
WMS_DATABASE=:memory: QT_SCALE_FACTOR=1 XDG_CONFIG_HOME="$config" \
    QT_FORCE_STDERR_LOGGING=1 QT_QPA_PLATFORM=offscreen \
    timeout 120 "${WMS_BUILD_DIR:-build}/tools/shots/wms_shots" "$dir"
echo "$(ls "$dir" | wc -l) shots in $dir"
