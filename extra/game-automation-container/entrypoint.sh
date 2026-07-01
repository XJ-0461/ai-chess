#!/usr/bin/env bash
#
# entrypoint.sh - bring up the headless environment, then hand off to the match
# orchestrator.
#
# Responsibilities (kept deliberately thin — all match logic lives in
# orchestrate.py):
#   1. Set up the output directory and mirror all output to container_execution.log
#   2. Start a virtual X display (Xvfb) sized for the 1920x1080 spectator window
#   3. Start a PulseAudio virtual sink so game audio can be captured
#   4. exec orchestrate.py
#
set -euo pipefail

OUTPUT_DIRECTORY="${OUTPUT_DIRECTORY:-/output}"
LOG_FILE="${OUTPUT_DIRECTORY}/container_execution.log"

mkdir -p "${OUTPUT_DIRECTORY}/camera" "${OUTPUT_DIRECTORY}/gaunt_stream"

# Mirror everything (stdout + stderr) from here on into the execution log while
# still printing to the container console.
exec > >(tee -a "${LOG_FILE}") 2>&1

log() { echo "[entrypoint $(date -u '+%H:%M:%S')] $*"; }

log "Container starting. OUTPUT_DIRECTORY=${OUTPUT_DIRECTORY}"

# --- Virtual display ---------------------------------------------------------
# Force X11 (SDL default may pick a non-existent driver headless); the X11 XID
# path used for window capture requires the x11 SDL video driver.
export DISPLAY=":99"
export SDL_VIDEO_DRIVER="x11"
# The framebuffer is exactly the spectator window size; the orchestrator raises
# the floating spectator window to fill it, and ffmpeg grabs the whole screen.
XVFB_RESOLUTION="${XVFB_RESOLUTION:-1920x1080x24}"

log "Starting Xvfb on ${DISPLAY} (${XVFB_RESOLUTION})"
Xvfb "${DISPLAY}" -screen 0 "${XVFB_RESOLUTION}" -nolisten tcp &
XVFB_PID=$!

# Wait for the display to accept connections.
for _ in $(seq 1 50); do
    if xdpyinfo -display "${DISPLAY}" >/dev/null 2>&1; then
        break
    fi
    sleep 0.2
done
if ! xdpyinfo -display "${DISPLAY}" >/dev/null 2>&1; then
    log "ERROR: Xvfb failed to come up on ${DISPLAY}"
    exit 1
fi
log "Xvfb ready (pid ${XVFB_PID})"

# --- Virtual audio sink ------------------------------------------------------
# PulseAudio in system mode with a null sink; the game plays into 'chess' and
# ffmpeg captures 'chess.monitor'. auth-anonymous lets the root-run clients
# connect without pulse-access group membership. Audio is best-effort: if this
# fails, orchestrate.py detects the missing monitor and records video-only.
export SDL_AUDIODRIVER="pulse"
export PULSE_SERVER="unix:/tmp/pulse/native"
mkdir -p /tmp/pulse

log "Starting PulseAudio virtual sink"
if pulseaudio --system --disallow-exit --disable-shm --exit-idle-time=-1 \
        -L "module-native-protocol-unix auth-anonymous=1 socket=/tmp/pulse/native" \
        -L "module-null-sink sink_name=chess sink_properties=device.description=Chess" \
        -L "module-always-sink" & then
    PULSE_PID=$!
    # Give the daemon a moment and set the null sink as default.
    for _ in $(seq 1 25); do
        if pactl info >/dev/null 2>&1; then
            break
        fi
        sleep 0.2
    done
    if pactl info >/dev/null 2>&1; then
        pactl set-default-sink chess >/dev/null 2>&1 || true
        log "PulseAudio ready (pid ${PULSE_PID}); default sink 'chess'"
    else
        log "WARNING: PulseAudio did not become ready; continuing (video-only capture)"
    fi
else
    log "WARNING: PulseAudio failed to start; continuing (video-only capture)"
fi

export OUTPUT_DIRECTORY

log "Handing off to orchestrate.py"
exec python3 /app/orchestrate.py
