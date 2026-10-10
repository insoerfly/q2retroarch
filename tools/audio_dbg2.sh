#!/bin/sh
# audio_dbg2.sh - run on the Q2/A13 target. Frees the ALSA PCM (RetroArch holds it
# in PREPARED state), then captures codec registers across a REAL continuous
# playback. All output -> /flash/AUDIODBG2.LOG (console stays quiet).
#
LOG=/flash/AUDIODBG2.LOG
REG=/flash/REGS.log
BIN=/flash/codecreg

exec > "$LOG" 2>&1

echo "=== audio_dbg2 $(date) ==="

echo "--- pcm owner/status (before) ---"
cat /proc/asound/card0/pcm0p/sub0/status

echo "--- stopping retroarch to free PCM ---"
killall retroarch 2>/dev/null
sleep 1
killall -9 retroarch 2>/dev/null
sleep 1
cat /proc/asound/card0/pcm0p/sub0/status

echo "--- start continuous codec capture (50ms) ---"
rm -f "$REG"
"$BIN" 0 50 > "$REG" 2>&1 &
CP=$!
sleep 1

echo "--- start playback: speaker-test sine 1000Hz ---"
speaker-test -D default -t sine -f 1000 -c 2 -l 30 >/dev/null 2>&1 &
SP=$!

sleep 3
kill $SP 2>/dev/null
sleep 1
kill $CP 2>/dev/null
sleep 0.3

echo "--- codec regs captured ---"
cat "$REG"

echo "--- pcm status (after) ---"
cat /proc/asound/card0/pcm0p/sub0/status

echo "--- dmesg tail ---"
dmesg | tail -20
echo "=== done ==="
