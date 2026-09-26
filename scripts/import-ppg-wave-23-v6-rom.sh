#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
archive_path=${1:-"$project_dir/PPG Wave 2.3 version 6.zip"}
destination_dir=${2:-"$project_dir/Firmware/wave_sys1_700"}
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM

unzip -q "$archive_path" -d "$work_dir"

for image in w23_64.bin w23_66.bin w23_68.bin; do
    image_path="$work_dir/v6/$image"
    if [ ! -f "$image_path" ] || [ "$(wc -c < "$image_path" | tr -d ' ')" != 8192 ]; then
        printf '%s\n' "Missing or invalid $image in $archive_path" >&2
        exit 1
    fi
done

mkdir -p "$destination_dir"
output_path="$destination_dir/ppg-wave-2.3-v6-wavetables.rom"
cp "$work_dir/v6/w23_64.bin" "$output_path"
dd if="$work_dir/v6/w23_66.bin" of="$output_path" bs=8192 count=1 seek=1 conv=notrunc status=none

# The two wavetable EPROMs occupy 16 KiB. V6 wavetable 13 deliberately
# addresses two wave slots beyond that range, where the original hardware
# exposes bytes from the adjacent program ROM. Retaining the first 768 bytes
# of w23_68 reproduces that documented V6 edge case and produces the complete
# 768-byte directory + 256 x 64-byte half-wave research image.
dd if="$work_dir/v6/w23_68.bin" of="$output_path" bs=1 count=768 seek=16384 conv=notrunc status=none

printf '%s\n' "Imported private PPG Wave 2.3 V6 wavetable image:"
printf '%s\n' "$output_path"
