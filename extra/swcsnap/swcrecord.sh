#!/bin/sh
# swcrecord - screen recording using swcsnap and ffmpeg
# Usage: swcrecord [output.mp4]

OUTPUT="${1:-recording.mp4}"

echo "recording to $OUTPUT (ctrl+c to stop)..."

while swcsnap; do :; done | ffmpeg -y -use_wallclock_as_timestamps 1 -f image2pipe -i - \
	-c:v libx264 -preset ultrafast -crf 23 -pix_fmt yuv420p \
	"$OUTPUT" 2>/dev/null

echo "saved to $OUTPUT"
