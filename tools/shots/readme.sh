#!/bin/bash
# The README screenshots (docs/screenshots/<screen>-light|dark.png): a populated day in
# season, 1280x800 at 2x for sharp text, framed by frame.py. Run from the repository root:
#   tools/shots/readme.sh
# Needs faketime (the day is 14.07.2026, the clock runs 20x so receipt times differ) and
# Pillow. Pallet weights come from the scale simulator, so every run differs a little.
set -euo pipefail
command -v faketime >/dev/null || { echo "readme.sh: needs faketime" >&2; exit 1; }
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/config" "$work/pdf"
WMS_DATABASE=:memory: QT_SCALE_FACTOR=2 XDG_CONFIG_HOME="$work/config" WMS_PRINT_PDF="$work/pdf" TZ=Europe/Kyiv \
    QT_FORCE_STDERR_LOGGING=1 QT_QPA_PLATFORM=offscreen FAKETIME_DONT_FAKE_MONOTONIC=1 \
    timeout 180 faketime -f "@2026-07-14 13:05:00 x20" \
    "${WMS_BUILD_DIR:-build}/tools/shots/wms_shots" --readme "$work/raw"
python3 "$(dirname "$0")/frame.py" "$work/raw" docs/screenshots >/dev/null
echo "$(ls "$work/raw" | wc -l) shots in docs/screenshots"
