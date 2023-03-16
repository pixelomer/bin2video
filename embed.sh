#!/usr/bin/env bash

DATA_HEIGHT=100
FRAME_REPEAT=2
BITS_PER_PIXEL=1

if [ -z "$1" -o -z "$2" -o -z "$3" ]; then
	echo "Usage: $0 <file.bin> <content.mp4> <output.mp4>"
	echo "  Encodes the given file.bin and merges it together with content.mp4"
	echo "  to create output.mp4. The resulting file can be decoded with bin2video."
	exit 1
fi

binary="$1"
video_in="$2"
video_out="$3"

ffprobe_output="$(ffprobe -v error -select_streams v:0 -show_entries stream=width,height -of default=nw=1:nk=1 "${video_in}")"
resolution=(${ffprobe_output})
width="${resolution[0]}"
height="${resolution[1]}"

in_args=()
if [ "${binary}" != "-" ]; then
	in_args=(-i "${binary}")
fi

set -x
./bin2video -e -E -f -1 -b "${BITS_PER_PIXEL}" -c "${FRAME_REPEAT}" -h "$((height + DATA_HEIGHT))" -w "${width}" -H "${DATA_HEIGHT}" "${in_args[@]}" -F "-i ${video_in} -filter_complex overlay=x=0:y=main_h-overlay_h -c:a copy -pix_fmt yuv420p -c:v libx264" -o "${video_out}"