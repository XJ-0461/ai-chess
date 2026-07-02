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

# Start each run with a fresh log (the volume persists across runs, so otherwise
# it accumulates every previous run's output).
: > "${LOG_FILE}"

# Mirror everything (stdout + stderr) from here on into the execution log while
# still printing to the container console. stdbuf -oL -eL forces line buffering
# so nothing is lost if the script (or a child) dies — otherwise tee's pipe can
# swallow the final, most important lines on an abrupt exit.
exec > >(stdbuf -oL -eL tee -a "${LOG_FILE}") 2>&1

log() { echo "[entrypoint $(date -u '+%H:%M:%S')] $*"; }

# Always record why we exited. Without this, a `set -e` abort (or any non-zero
# exit) leaves no trace and the container just vanishes.
on_exit() {
    rc=$?
    log "entrypoint exiting (rc=${rc})"
    # Give the tee subshell a moment to flush the last line to disk.
    sync || true
}
trap on_exit EXIT

# Surface the exact command + line number that trips `set -e`.
trap 'log "ERROR: command failed (rc=$?) at line ${LINENO}: ${BASH_COMMAND}"' ERR

log "Container starting. OUTPUT_DIRECTORY=${OUTPUT_DIRECTORY}"

# --- Virtual display ---------------------------------------------------------
# Force X11 (SDL default may pick a non-existent driver headless); the X11 XID
# path used for window capture requires the x11 SDL video driver.
export DISPLAY=":99"
export SDL_VIDEO_DRIVER="x11"
# The framebuffer is exactly the spectator window size; the orchestrator raises
# the floating spectator window to fill it, and ffmpeg grabs the whole screen.
XVFB_RESOLUTION="${XVFB_RESOLUTION:-1920x1080x24}"

# Ensure the X11 socket directory exists (minimal images may not ship it).
mkdir -p /tmp/.X11-unix && chmod 1777 /tmp/.X11-unix

log "Starting Xvfb on ${DISPLAY} (${XVFB_RESOLUTION})"
# Log Xvfb's own stderr to a file so a crash-on-startup (e.g. missing GLX,
# bad screen spec) is captured instead of lost.
XVFB_LOG="${OUTPUT_DIRECTORY}/xvfb.log"
Xvfb "${DISPLAY}" -screen 0 "${XVFB_RESOLUTION}" -nolisten tcp >"${XVFB_LOG}" 2>&1 &
XVFB_PID=$!

# Wait for the display to accept connections. Readiness = Xvfb process alive AND
# its X11 socket present (this is the authoritative signal that the server is up;
# xdotool is validated separately below because it's the tool the orchestrator
# relies on and its failures must be seen, not swallowed).
X_SOCKET="/tmp/.X11-unix/X${DISPLAY#:}"
xvfb_ready=0
for _ in $(seq 1 50); do
    if ! kill -0 "${XVFB_PID}" 2>/dev/null; then
        log "ERROR: Xvfb process (pid ${XVFB_PID}) died during startup"
        log "----- Xvfb log -----"
        cat "${XVFB_LOG}" 2>/dev/null || true
        log "--------------------"
        exit 1
    fi
    if [ -S "${X_SOCKET}" ]; then
        xvfb_ready=1
        break
    fi
    sleep 0.2
done
if [ "${xvfb_ready}" -ne 1 ]; then
    log "ERROR: Xvfb failed to create its socket ${X_SOCKET} on ${DISPLAY}"
    log "----- Xvfb log -----"
    cat "${XVFB_LOG}" 2>/dev/null || true
    log "--------------------"
    exit 1
fi
log "Xvfb socket present (pid ${XVFB_PID})"

# Validate that xdotool can actually talk to the display. If this fails the whole
# run is doomed (the orchestrator uses xdotool to move/raise the spectator
# window), so surface the real error instead of hiding it behind /dev/null.
# Run it as an `if` condition so a non-zero rc doesn't trip `set -e` before we log.
if xdotool_out="$(xdotool getdisplaygeometry 2>&1)"; then
    log "Xvfb ready (pid ${XVFB_PID}); geometry=${xdotool_out}"
else
    log "ERROR: xdotool cannot talk to ${DISPLAY} (rc=$?): ${xdotool_out}"
    exit 1
fi

# --- Virtual audio sink ------------------------------------------------------
# PulseAudio as a plain per-user daemon (NOT --system): system mode drops
# privileges to the 'pulse' user, which then can't bind our socket under /tmp
# and needs a D-Bus system bus we don't have — that produced a wall of errors.
# A per-user daemon (we're root) with -n (skip the stock config, so no autospawn
# of D-Bus/udev modules) loads exactly the two modules we want: a null sink the
# game plays into ('chess') and its monitor source ffmpeg captures. Its own
# stdout/stderr go to a dedicated log so any residual warnings never pollute
# container_execution.log. Audio is best-effort: if it doesn't come up,
# orchestrate.py detects the missing monitor and records video-only.
# The statically-linked SDL3 in the Chess binary was built with the ALSA backend
# but NOT the PulseAudio one (it reports "Drivers: alsa disk dummy"), so we can't
# ask SDL to talk to Pulse directly. Instead we drive SDL's ALSA backend and route
# ALSA's default device into our Pulse null sink via the alsa-pulse plugin. The
# game -> ALSA(default) -> Pulse 'chess' sink -> chess.monitor -> ffmpeg.
export SDL_AUDIODRIVER="alsa"
export PULSE_SERVER="unix:/tmp/pulse/native"
export XDG_RUNTIME_DIR="/tmp/xdg"
export HOME="${HOME:-/root}"
mkdir -p /tmp/pulse "${XDG_RUNTIME_DIR}"
chmod 700 "${XDG_RUNTIME_DIR}"
PULSE_LOG="${OUTPUT_DIRECTORY}/pulseaudio.log"

# Make ALSA's default PCM/CTL the pulse plugin, pointed at our Pulse socket.
cat > /etc/asound.conf <<'ASOUNDCONF'
pcm.!default {
    type pulse
    server "unix:/tmp/pulse/native"
}
ctl.!default {
    type pulse
    server "unix:/tmp/pulse/native"
}
ASOUNDCONF

log "Starting PulseAudio virtual sink (per-user mode; see pulseaudio.log)"
pulseaudio \
        --daemonize=no \
        --fail=false \
        --exit-idle-time=-1 \
        --disable-shm=true \
        -n \
        -L "module-native-protocol-unix auth-anonymous=1 socket=/tmp/pulse/native" \
        -L "module-null-sink sink_name=chess sink_properties=device.description=Chess" \
        -L "module-always-sink" \
        >"${PULSE_LOG}" 2>&1 &
PULSE_PID=$!

# Wait for the daemon to accept connections on our socket.
pulse_ready=0
for _ in $(seq 1 25); do
    if pactl info >/dev/null 2>&1; then
        pulse_ready=1
        break
    fi
    sleep 0.2
done
if [ "${pulse_ready}" -eq 1 ]; then
    pactl set-default-sink chess >/dev/null 2>&1 || true
    log "PulseAudio ready (pid ${PULSE_PID}); default sink 'chess'"
else
    log "WARNING: PulseAudio did not become ready; continuing (video-only capture)"
fi

export OUTPUT_DIRECTORY

log "Handing off to orchestrate.py"
exec python3 /app/orchestrate.py
