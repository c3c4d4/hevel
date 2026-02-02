#!/bin/sh
# swcrecord - screen recording using swcsnap and ffmpeg
# Usage: swcrecord [output.mp4] [fps]

OUTPUT="${1:-recording.mp4}"
FPS="${2:-30}"
FRAME_DELAY=$(awk "BEGIN {print 1/$FPS}")
TMPDIR=$(mktemp -d)
COUNTFILE="$TMPDIR/count"
STARTFILE="$TMPDIR/start"
echo "0" > "$COUNTFILE"

cleanup() {
	END_TIME=$(date +%s.%N)
	START_TIME=$(cat "$STARTFILE")
	FRAME_COUNT=$(cat "$COUNTFILE")

	echo ""
	echo "encoding video from $FRAME_COUNT frames..."

	if [ "$FRAME_COUNT" -gt 0 ] && [ -f "$TMPDIR/frame_000001.ppm" ]; then
		DURATION=$(awk "BEGIN {print $END_TIME - $START_TIME}")
		ACTUAL_FPS=$(awk "BEGIN {printf \"%.2f\", $FRAME_COUNT / $DURATION}")
		echo "actual capture rate: ${ACTUAL_FPS}fps over ${DURATION}s"

		ffmpeg -y -framerate "$ACTUAL_FPS" -i "$TMPDIR/frame_%06d.ppm" \
			-c:v libx264 -preset ultrafast -crf 23 -pix_fmt yuv420p \
			"$OUTPUT"
		echo "saved to $OUTPUT"
	else
		echo "no frames captured"
	fi
	rm -rf "$TMPDIR"
	exit 0
}

trap cleanup INT TERM

date +%s.%N > "$STARTFILE"
echo "recording to $OUTPUT at ${FPS}fps (ctrl+c to stop)..."

while true; do
	FRAME_COUNT=$(cat "$COUNTFILE")
	FRAME_COUNT=$((FRAME_COUNT + 1))
	echo "$FRAME_COUNT" > "$COUNTFILE"
	swcsnap > "$TMPDIR/frame_$(printf '%06d' $FRAME_COUNT).ppm"
	sleep "$FRAME_DELAY"
done
