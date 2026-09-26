#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
archive_path="$project_dir/Firmware/system.zip"
extract_path="$project_dir/Firmware/download"

mkdir -p "$extract_path"
curl -L --fail --show-error \
  'https://downloads.waldorfmusic.com/cloud/index.php/s/7zzTnmzK2pYY9Ks/download' \
  -o "$archive_path"
unzip -o "$archive_path" -d "$extract_path"
unzip -o "$extract_path/System/wave_sys1_700.zip" -d "$project_dir/Firmware"

printf '%s\n' "Firmware downloaded from Waldorf Music to:"
printf '%s\n' "$project_dir/Firmware/wave_sys1_700"
