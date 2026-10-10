#!/bin/sh
# audio_dbg.sh - run on the Q2/A13 target (card). Dumps the sunxi audio-codec
# registers (via /flash/codecreg) before, during and after playback.
#
# Usage (on device, as root):  sh /flash/audio_dbg.sh
#
BIN=/flash/codecreg
WAV=/flash/button_1.wav
LOG=/flash/AUDIODBG.LOG

log() { echo "$@" | tee -a "$LOG"; }

: > "$LOG"
log "=== audio_dbg $(date) ==="

log "--- /proc/asound/cards ---"
cat /proc/asound/cards 2>&1 | tee -a "$LOG"

log "--- mixer (DAC) ---"
amixer -c 0 contents 2>/dev/null | grep -iE "name|value" | tee -a "$LOG"

log "--- codec regs BEFORE playback ---"
"$BIN" 3 200 2>&1 | tee -a "$LOG"

# start a continuous tone / wav in the background
PLAY=""
if command -v aplay >/dev/null 2>&1 && [ -f "$WAV" ]; then
	PLAY="aplay -D default $WAV"
elif command -v speaker-test >/dev/null 2>&1; then
	PLAY="speaker-test -D default -t sine -f 440 -c 2"
fi
log "--- starting playback: $PLAY ---"
if [ -n "$PLAY" ]; then
	$PLAY >/dev/null 2>&1 &
	PID=$!
	sleep 1
	log "--- codec regs DURING playback (20 samples @150ms) ---"
	"$BIN" 20 150 2>&1 | tee -a "$LOG"
	kill $PID 2>/dev/null
	wait $PID 2>/dev/null
else
	log "!!! no aplay/speaker-test available"
fi

log "--- codec regs AFTER playback ---"
"$BIN" 2 200 2>&1 | tee -a "$LOG"

log "--- dmesg tail ---"
dmesg 2>/dev/null | tail -30 | tee -a "$LOG"

log "=== done; saved to $LOG ==="
echo "RESULT: $LOG"
