#!/bin/bash
# Screenshots of every screen into <dir>, in a clean environment: a database in memory
# (demo data), scale 100%, and settings in a temporary folder, so the operator's own
# QSettings are neither read nor changed. Run from the repository root after a build:
#   tools/shots/shots.sh /tmp/before
#   ... change the UI, rebuild ...
#   tools/shots/shots.sh /tmp/after && python3 tools/shots/compare.py /tmp/before /tmp/after
#
# The clock starts at the same moment every run (libfaketime: package faketime on Ubuntu,
# libfaketime on Fedora), so receipt times and the session date match between runs. The
# terminal reads the clock in many places, the core included; faking it for the process
# keeps the code free of a test-only clock. Monotonic time stays real: Qt's timers need it.
# Without faketime the shots are still taken, and the clock shows up in compare.py.
set -euo pipefail
dir=${1:?usage: shots.sh <dir>}
config=$(mktemp -d)
trap 'rm -rf "$config"' EXIT
rm -rf "$dir"

clock=()
if command -v faketime >/dev/null; then
    # "@" starts the clock there and lets it run; a run takes well under a minute.
    clock=(faketime -f "@2026-11-13 10:00:00")
    export FAKETIME_DONT_FAKE_MONOTONIC=1
else
    echo "shots.sh: faketime not installed, the real clock will differ between runs" >&2
fi

WMS_DATABASE=:memory: QT_SCALE_FACTOR=1 XDG_CONFIG_HOME="$config" TZ=Europe/Kyiv \
    QT_FORCE_STDERR_LOGGING=1 QT_QPA_PLATFORM=offscreen \
    timeout 120 "${clock[@]}" "${WMS_BUILD_DIR:-build}/tools/shots/wms_shots" "$dir"
echo "$(ls "$dir" | wc -l) shots in $dir"
