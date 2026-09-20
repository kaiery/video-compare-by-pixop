#!/usr/bin/env bash
set -euo pipefail
# Headless SDL events only; no global keyboard/mouse automation.
test_binary="$1"
temp_root=$(cd "${TMPDIR:-/tmp}" && pwd -P)
media=$(mktemp -d "$temp_root/video-compare-frame-step.XXXXXXXX")
cleanup() {
  local resolved
  resolved=$(cd "$media" && pwd -P) || return
  if [[ "$resolved" == "$temp_root"/video-compare-frame-step.* && "${resolved%/*}" == "$temp_root" ]]; then
    rm -rf -- "$resolved"
  fi
}
trap cleanup EXIT
ffmpeg -hide_banner -loglevel error -y -f lavfi -i 'testsrc2=size=160x96:rate=25:duration=3' \
  -c:v libx264 -g 50 -bf 3 -pix_fmt yuv420p "$media/left.mp4"
ffmpeg -hide_banner -loglevel error -y -i "$media/left.mp4" -vf hflip \
  -c:v libx264 -g 50 -bf 3 "$media/right.mp4"
ffmpeg -hide_banner -loglevel error -y -f lavfi -i 'testsrc2=size=192x128:rate=30:duration=4' \
  -c:v libx264 -g 60 -bf 3 "$media/right30.mp4"
ffmpeg -hide_banner -loglevel error -y -f lavfi -i 'testsrc2=size=160x96:rate=25:duration=2' \
  -vf 'setpts=if(lt(N\,25)\,N/(25*TB)\,(1+(N-25)/10)/TB)' -fps_mode vfr \
  -c:v libx264 -g 50 -bf 3 "$media/vfr.mp4"
for name in left vfr; do
  ffprobe -v error -select_streams v:0 -show_entries frame=best_effort_timestamp_time \
    -of csv=p=0 "$media/$name.mp4" | awk -F, '$1 ~ /^[0-9]+\.[0-9]+$/ {print $1}' > "$media/$name-pts.txt"
done
mkdir "$media/cfr" "$media/shared" "$media/vfr-multi" "$media/default-cache"
(cd "$media/default-cache"; FRAME_STEP_MAX_REVERSE_SEEKS=2 "$test_binary" 50 "$media/left-pts.txt" "$media/left.mp4" "$media/right.mp4")
(cd "$media/cfr"; FRAME_STEP_MAX_REVERSE_SEEKS=50 "$test_binary" 3 "$media/left-pts.txt" "$media/left.mp4" "$media/right.mp4")
(cd "$media/shared"; "$test_binary" 1 "$media/left-pts.txt" "$media/left.mp4" "$media/left.mp4")
(cd "$media/vfr-multi"; FRAME_STEP_TRUST_PTS=1 "$test_binary" 3 "$media/vfr-pts.txt" "$media/vfr.mp4" "$media/right30.mp4" "$media/right30.mp4")
# Opt-in resolution check; keeps the regular regression suite small.
if [[ "${FRAME_STEP_HD:-0}" == 1 ]]; then
  ffmpeg -hide_banner -loglevel error -y -f lavfi -i 'testsrc2=size=1280x720:rate=25:duration=3' \
    -c:v libx264 -g 50 -bf 3 "$media/hd-left.mp4"
  ffmpeg -hide_banner -loglevel error -y -i "$media/hd-left.mp4" -vf hflip \
    -c:v libx264 -g 50 -bf 3 "$media/hd-right.mp4"
  mkdir "$media/hd"
  (cd "$media/hd"; FRAME_STEP_MAX_REVERSE_SEEKS=2 "$test_binary" 50 "$media/left-pts.txt" "$media/hd-left.mp4" "$media/hd-right.mp4")
fi
