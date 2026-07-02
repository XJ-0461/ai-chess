#!/usr/bin/env python3
"""
orchestrate.py - drive one AI chess match end-to-end inside the container.

Responsibilities:
  1. Parse the runtime inputs (THRASH_CONFIG, THRASH_STREAM_ID, OPENROUTER_API_KEY).
  2. Launch the two Node chess agents and the C++ Chess game.
  3. Drive the game over its ZMQ command server one command at a time, reading
     each response — in particular the X11 window id returned by
     open_spectator_view, used to target the screen recorder.
  4. Record the spectator window (video + audio) with ffmpeg x11grab.
  5. Poll query_match_result until the match concludes.
  6. Convert the capture to HLS (2s segments) and write result.json.

Everything is wrapped so the game/agent/ffmpeg processes are always reaped and a
result.json is always written (success=false on any unexpected failure).
"""

import json
import logging
import os
import shutil
import signal
import subprocess
import sys
import time
import uuid

import zmq

# --- Configuration -----------------------------------------------------------
OUTPUT_DIRECTORY = os.environ.get("OUTPUT_DIRECTORY", "/output")
DISPLAY = os.environ.get("DISPLAY", ":99")

CHESS_BINARY = "/app/Chess"
AGENT_ENTRY = "/app/chess-agent/dist/index.js"

# The render loop's only frame throttle is vsync, which is a no-op under Xvfb +
# llvmpipe (no vblank), so it free-runs and the software rasterizer pins every
# core. We run the game under MangoHud, whose OpenGL layer caps the buffer-swap
# rate via its fps_limit (with the overlay hidden, no_display=1).
CHESS_FPS_LIMIT = os.environ.get("CHESS_FPS_LIMIT", "60")

WHITE_PORT = 5555
BLACK_PORT = 5556
COMMAND_PORT = 5599

CAMERA_DIR = os.path.join(OUTPUT_DIRECTORY, "camera")
GAUNT_DIR = os.path.join(OUTPUT_DIRECTORY, "gaunt_stream")
RESULT_JSON = os.path.join(OUTPUT_DIRECTORY, "result.json")
CAPTURE_FILE = "/tmp/capture.mkv"
HLS_PLAYLIST = os.path.join(CAMERA_DIR, "chess_match.m3u8")
HLS_SEGMENTS = os.path.join(CAMERA_DIR, "chess_match_%04d.ts")

PULSE_MONITOR = "chess.monitor"

# The command server blocks up to ~30s per command (its kCommandTimeout); give
# the client a little more headroom before it gives up on a reply.
COMMAND_TIMEOUT_S = 45
# Concluded outcomes that count as a real (non-void) result.
CONCLUDED_OK = {"white_win", "black_win", "draw"}
# Poll cadence and an overall safety cap so a hung game can't loop forever.
POLL_INTERVAL_S = 60
POST_WIN_HOLD_S = 60
MAX_MATCH_SECONDS = 2 * 60 * 60

logging.basicConfig(
    level=logging.INFO,
    format="[orchestrator %(asctime)s] %(message)s",
    datefmt="%H:%M:%S",
    stream=sys.stdout,
)
log = logging.getLogger("orchestrate")

# Processes to reap on exit.
_processes = {}


# --- Small helpers -----------------------------------------------------------
def run_quiet(args):
    """Run a command, returning (rc, stdout+stderr); never raises."""
    try:
        p = subprocess.run(args, capture_output=True, text=True, timeout=15)
        return p.returncode, (p.stdout or "") + (p.stderr or "")
    except Exception as exc:  # noqa: BLE001
        return 1, str(exc)


def parse_config():
    raw = os.environ.get("THRASH_CONFIG", "")
    stream_id = os.environ.get("THRASH_STREAM_ID", "").strip()
    api_key = os.environ.get("OPENROUTER_API_KEY", "").strip()

    if not raw:
        raise ValueError("THRASH_CONFIG is not set")
    if not stream_id:
        raise ValueError("THRASH_STREAM_ID is not set")
    if not api_key:
        raise ValueError("OPENROUTER_API_KEY is not set")

    config = json.loads(raw)
    actors = config.get("actors", {})
    white_model = actors.get("white")
    black_model = actors.get("black")
    if not white_model or not black_model:
        raise ValueError("THRASH_CONFIG.actors must contain 'white' and 'black' model ids")

    scenario = config.get("scenario", {})
    # The external config uses plural keys; the C++ configure_game expects singular.
    flags = {
        "enable_draw_offer": bool(scenario.get("enable_draw_offers", False)),
        "enable_resignation": bool(scenario.get("enable_resignations", False)),
        "enable_quip": bool(scenario.get("enable_quips", False)),
    }
    return {
        "stream_id": stream_id,
        "api_key": api_key,
        "white_model": white_model,
        "black_model": black_model,
        "flags": flags,
    }


# --- Process launch ----------------------------------------------------------
def launch_process(name, args, env=None):
    log.info("Launching %s: %s", name, " ".join(args))
    proc = subprocess.Popen(args, env=env)
    _processes[name] = proc
    return proc


def launch_agents():
    launch_process(
        "white-agent",
        ["node", AGENT_ENTRY, "--name", "White Agent",
         "--endpoint", f"tcp://127.0.0.1:{WHITE_PORT}"],
    )
    launch_process(
        "black-agent",
        ["node", AGENT_ENTRY, "--name", "Black Agent",
         "--endpoint", f"tcp://127.0.0.1:{BLACK_PORT}"],
    )
    # The agents bind their sockets; give them a moment to come up.
    time.sleep(2)


def launch_game(gaunt_output):
    chess_cmd = [
        CHESS_BINARY,
        "--command-server-endpoint", f"tcp://127.0.0.1:{COMMAND_PORT}",
        "--white-endpoint", f"tcp://127.0.0.1:{WHITE_PORT}",
        "--black-endpoint", f"tcp://127.0.0.1:{BLACK_PORT}",
        "--gaunt-telemetry-xml-output-file", gaunt_output,
    ]
    # Cap the render loop's FPS with MangoHud so the software rasterizer can't
    # saturate every core. Fall back to an uncapped launch if mangohud is missing
    # (e.g. running the orchestrator locally outside the container).
    env = None
    if shutil.which("mangohud"):
        chess_cmd = ["mangohud", *chess_cmd]
        env = dict(os.environ)
        # no_display=1 hides the overlay so it never renders into the capture.
        env["MANGOHUD_CONFIG"] = f"fps_limit={CHESS_FPS_LIMIT},no_display=1"
    else:
        log.warning("mangohud not found; running Chess without an FPS cap")
    launch_process("chess", chess_cmd, env=env)


def terminate_all():
    for name, proc in list(_processes.items()):
        if proc.poll() is not None:
            continue
        log.info("Terminating %s (pid %d)", name, proc.pid)
        try:
            proc.terminate()
            proc.wait(timeout=10)
        except Exception:  # noqa: BLE001
            try:
                proc.kill()
            except Exception:  # noqa: BLE001
                pass


# --- ZMQ command client ------------------------------------------------------
class CommandClient:
    def __init__(self, endpoint):
        self.ctx = zmq.Context.instance()
        self.sock = self.ctx.socket(zmq.DEALER)
        self.sock.setsockopt(zmq.LINGER, 0)
        self.sock.connect(endpoint)
        self.poller = zmq.Poller()
        self.poller.register(self.sock, zmq.POLLIN)

    def send(self, ctype, detail=None, timeout_s=COMMAND_TIMEOUT_S):
        detail = detail or {}
        cid = uuid.uuid4().hex
        self.sock.send_json({"type": ctype, "detail": detail, "id": cid})
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            remaining = int(max(0, (deadline - time.monotonic())) * 1000)
            if dict(self.poller.poll(timeout=remaining)):
                resp = self.sock.recv_json()
                # Ignore stale replies from a previous (timed-out) command.
                if resp.get("id") in (cid, None) or "id" not in resp:
                    return resp
        raise TimeoutError(f"command '{ctype}' timed out after {timeout_s}s")

    def wait_for_server(self, game_proc, timeout_s=90):
        """Block until the command server answers, or the game dies."""
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            if game_proc.poll() is not None:
                raise RuntimeError("Chess process exited before its command server was ready")
            try:
                # A query for a non-existent game returns immediately ("unknown").
                self.send("query_match_result", {"game_id": "__ping__"}, timeout_s=3)
                return
            except TimeoutError:
                continue
        raise TimeoutError("command server never became ready")

    def close(self):
        try:
            self.sock.close(0)
        except Exception:  # noqa: BLE001
            pass


def extract_x11_window_id(response):
    """Pull the X11 XID out of an open_spectator_view response, or None."""
    os_meta = (
        response.get("window_metadata", {}).get("os_metadata", {})
    )
    if os_meta.get("type") == "x11_window":
        return os_meta.get("detail", {}).get("window_id")
    return None


# --- Recording ---------------------------------------------------------------
_video_encoder = None


def _available_encoders():
    """Set of encoder names compiled into this ffmpeg build."""
    rc, out = run_quiet(["ffmpeg", "-hide_banner", "-encoders"])
    names = set()
    if rc == 0:
        for line in out.splitlines():
            parts = line.split()
            # Encoder lines look like: " V....D libx264   H.264 ...". The first
            # token is the capability flags (e.g. "V....D"); the second is the name.
            if len(parts) >= 2 and parts[0] and set(parts[0]) <= set("VASFXBD."):
                names.add(parts[1])
    return names


def pick_video_encoder():
    """Choose the best H.264-capable encoder available, cached after first call.

    Fedora's ffmpeg-free ships WITHOUT libx264 (patent-stripped), so we probe for
    what's actually present. Preference: real H.264 (best HLS/browser support),
    then mpeg2video as a universally-available MPEG-TS-compatible last resort.
    """
    global _video_encoder
    if _video_encoder is not None:
        return _video_encoder
    avail = _available_encoders()
    for name in ("libx264", "libopenh264", "h264_vaapi", "mpeg2video"):
        if name in avail:
            _video_encoder = name
            break
    else:
        _video_encoder = "mpeg2video"
    log.info("Selected video encoder: %s", _video_encoder)
    return _video_encoder


def video_encoder_args(enc):
    """ffmpeg -c:v flags tuned per encoder (they don't share option names)."""
    if enc == "libx264":
        return ["-c:v", "libx264", "-preset", "veryfast", "-pix_fmt", "yuv420p"]
    if enc == "libopenh264":
        # libopenh264 has no -preset; drive it with a target bitrate instead.
        return ["-c:v", "libopenh264", "-b:v", "6M", "-pix_fmt", "yuv420p"]
    if enc == "mpeg2video":
        return ["-c:v", "mpeg2video", "-qscale:v", "4", "-pix_fmt", "yuv420p"]
    return ["-c:v", enc, "-pix_fmt", "yuv420p"]


def audio_available():
    rc, out = run_quiet(["pactl", "list", "short", "sources"])
    return rc == 0 and PULSE_MONITOR in out


def start_recording():
    """Start ffmpeg capturing the framebuffer (+ audio if available)."""
    with_audio = audio_available()
    log.info("Recording with audio=%s", with_audio)

    args = [
        "ffmpeg", "-hide_banner", "-loglevel", "warning", "-y",
        "-f", "x11grab", "-draw_mouse", "0",
        "-framerate", "30", "-video_size", "1920x1080",
        "-i", f"{DISPLAY}+0,0",
    ]
    if with_audio:
        args += ["-f", "pulse", "-i", PULSE_MONITOR]
    args += video_encoder_args(pick_video_encoder())
    args += [
        # Force keyframes every 2s so the capture can be stream-copied into
        # aligned 2-second HLS segments afterwards.
        "-force_key_frames", "expr:gte(t,n_forced*2)",
    ]
    if with_audio:
        args += ["-c:a", "aac", "-b:a", "128k"]
    args += [CAPTURE_FILE]

    log.info("ffmpeg: %s", " ".join(args))
    proc = subprocess.Popen(args, stdin=subprocess.PIPE)
    _processes["ffmpeg"] = proc
    return proc


def stop_recording(proc):
    if proc.poll() is not None:
        log.warning("ffmpeg already exited (rc=%s) before stop", proc.returncode)
        return
    log.info("Stopping ffmpeg (graceful 'q')")
    try:
        proc.communicate(input=b"q", timeout=15)
    except Exception:  # noqa: BLE001
        try:
            proc.send_signal(signal.SIGINT)
            proc.wait(timeout=10)
        except Exception:  # noqa: BLE001
            proc.kill()


def raise_window(xid):
    """Move/size/raise the spectator window so it fills the framebuffer."""
    if not xid:
        log.warning("No X11 window id; capturing the whole framebuffer as-is")
        return
    hex_id = hex(int(xid))
    for cmd in (
        ["xdotool", "windowmove", hex_id, "0", "0"],
        ["xdotool", "windowsize", hex_id, "1920", "1080"],
        ["xdotool", "windowactivate", hex_id],
        ["xdotool", "windowraise", hex_id],
    ):
        rc, out = run_quiet(cmd)
        if rc != 0:
            log.warning("%s failed: %s", " ".join(cmd), out.strip())
    time.sleep(1)


def convert_to_hls():
    os.makedirs(CAMERA_DIR, exist_ok=True)
    base = [
        "ffmpeg", "-hide_banner", "-loglevel", "warning", "-y",
        "-i", CAPTURE_FILE,
    ]
    hls = [
        "-start_number", "0", "-hls_time", "2", "-hls_list_size", "0",
        "-hls_flags", "independent_segments",
        "-hls_segment_filename", HLS_SEGMENTS, "-f", "hls", HLS_PLAYLIST,
    ]
    # Prefer a lossless stream copy (keyframes were forced at 2s boundaries);
    # fall back to a re-encode if copy can't produce valid segments.
    rc, out = run_quiet(base + ["-c", "copy"] + hls)
    if rc == 0 and os.path.exists(HLS_PLAYLIST):
        log.info("HLS conversion done (stream copy)")
        return
    log.warning("Stream-copy HLS failed (%s); re-encoding", out.strip()[:400])
    rc, out = run_quiet(
        base + video_encoder_args(pick_video_encoder()) + ["-c:a", "aac"] + hls
    )
    if rc == 0 and os.path.exists(HLS_PLAYLIST):
        log.info("HLS conversion done (re-encode)")
    else:
        log.error("HLS conversion failed: %s", out.strip()[:400])


def write_result(success, cost_estimate=None):
    try:
        with open(RESULT_JSON, "w") as f:
            json.dump({
                "result": {"success": bool(success)},
                "model_cost_estimate": cost_estimate,
            }, f)
        log.info("Wrote %s: success=%s cost_estimate=%s",
                 RESULT_JSON, success, "present" if cost_estimate else "null")
    except Exception as exc:  # noqa: BLE001
        log.error("Failed to write result.json: %s", exc)


def query_cost_estimate(client, stream_id):
    """Query the game's per-side session cost estimate.

    Returns the {"white": ..., "black": ...} dict of raw float-USD spend, or
    None if the estimate could not be obtained (command error/timeout or a
    non-200 status such as an unknown game id)."""
    try:
        resp = client.send("game::estimate_cost", {"game_id": stream_id})
    except Exception as exc:  # noqa: BLE001
        log.warning("game::estimate_cost failed: %s", exc)
        return None

    if resp.get("status") != 200:
        log.warning("game::estimate_cost -> status=%s; no cost estimate",
                    resp.get("status"))
        return None

    def side(d):
        d = d or {}
        return {"input_token_cost": d.get("input", 0.0),
                "output_token_cost": d.get("output", 0.0)}

    return {"white": side(resp.get("estimated_white_cost")),
            "black": side(resp.get("estimated_black_cost"))}


# --- Main flow ---------------------------------------------------------------
def run_match():
    cfg = parse_config()
    stream_id = cfg["stream_id"]
    log.info("Match config: stream_id=%s white=%s black=%s flags=%s",
             stream_id, cfg["white_model"], cfg["black_model"], cfg["flags"])

    os.makedirs(CAMERA_DIR, exist_ok=True)
    os.makedirs(GAUNT_DIR, exist_ok=True)
    gaunt_output = os.path.join(GAUNT_DIR, f"{stream_id}.gaunt.stream.xml")

    launch_agents()
    launch_game(gaunt_output)

    client = CommandClient(f"tcp://127.0.0.1:{COMMAND_PORT}")
    client.wait_for_server(_processes["chess"])
    log.info("Command server ready")

    # 1. Provider
    client.send("configure_provider", {
        "name": "openrouter", "kind": "OpenRouter", "api_key": cfg["api_key"],
    })

    # 2. Game (agent endpoints + models + scenario flags)
    detail = {
        "game_id": stream_id,
        "white": {"endpoint": f"tcp://127.0.0.1:{WHITE_PORT}",
                  "provider": "OpenRouter", "model_id": cfg["white_model"]},
        "black": {"endpoint": f"tcp://127.0.0.1:{BLACK_PORT}",
                  "provider": "OpenRouter", "model_id": cfg["black_model"]},
        "retrospective_turn_count": 0,
    }
    detail.update(cfg["flags"])
    resp = client.send("configure_game", detail)
    if resp.get("code", 200) >= 400:
        raise RuntimeError(f"configure_game failed: {resp}")

    # 3. Spectator view -> X11 window id
    resp = client.send("open_spectator_view", {
        "game_id": stream_id,
        "game_view_theme": "default",
        "window_configuration": {
            "type": "floating",
            "window_size": {"width": 1920, "height": 1080},
            "window_position": {"x": 0, "y": 0},
            "imgui_flags": ["ImGuiWindowFlags_NoTitleBar"],
        },
    })
    xid = extract_x11_window_id(resp)
    imgui_window_id = (
        resp.get("window_metadata", {}).get("imgui_metadata", {}).get("window_id")
        or f"Spectator 1 - {stream_id}"
    )
    log.info("Spectator window: x11_id=%s imgui_id=%r", xid, imgui_window_id)

    # 4. Position/raise the window, then start recording.
    raise_window(xid)
    start_recording()

    # 5. Enable the move history bar.
    client.send("spectator_view::set_move_history_bar", {
        "window_id": imgui_window_id,
        "enable_move_history_bar": True,
    })

    # 6. Start the game.
    resp = client.send("start_game", {"game_id": stream_id})
    if resp.get("code", 200) >= 400:
        raise RuntimeError(f"start_game failed: {resp}")
    log.info("Game started; polling for result every %ds", POLL_INTERVAL_S)

    # 7. Poll for conclusion.
    outcome = "in_progress"
    started = time.monotonic()
    while True:
        try:
            resp = client.send("query_match_result", {"game_id": stream_id})
            outcome = resp.get("outcome", "unknown")
            log.info("query_match_result -> outcome=%s winner=%s cause=%s",
                     outcome, resp.get("winner"), resp.get("cause"))
        except TimeoutError as exc:
            log.warning("query_match_result timed out: %s", exc)
            outcome = "in_progress"

        if outcome not in ("in_progress", "unknown"):
            break
        if time.monotonic() - started > MAX_MATCH_SECONDS:
            log.error("Match exceeded %ds; giving up", MAX_MATCH_SECONDS)
            outcome = "timeout"
            break
        time.sleep(POLL_INTERVAL_S)

    success = outcome in CONCLUDED_OK
    log.info("Match concluded: outcome=%s -> success=%s", outcome, success)

    # 8. On a real conclusion, hold the recording briefly for the end animation.
    if success:
        log.info("Holding recording for %ds", POST_WIN_HOLD_S)
        time.sleep(POST_WIN_HOLD_S)

    # 9. Query the per-side cost estimate while the game process is still alive.
    cost_estimate = query_cost_estimate(client, stream_id)

    client.close()
    return success, cost_estimate


def main():
    success = False
    cost_estimate = None
    try:
        success, cost_estimate = run_match()
    except Exception as exc:  # noqa: BLE001
        log.exception("Match failed: %s", exc)
        success = False
        cost_estimate = None
    finally:
        ffmpeg = _processes.get("ffmpeg")
        if ffmpeg is not None:
            stop_recording(ffmpeg)
        terminate_all()
        if os.path.exists(CAPTURE_FILE):
            convert_to_hls()
        else:
            log.error("No capture file produced; camera output will be empty")
        write_result(success, cost_estimate)
        try:
            os.sync()
        except Exception:  # noqa: BLE001
            pass
    log.info("Done.")
    # Always exit 0: the deliverable is result.json, not the process exit code.
    sys.exit(0)


if __name__ == "__main__":
    main()
