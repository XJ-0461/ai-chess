# Game Automation Container

This is a container that runs one (1) game, logs its output telemetry (gaunt xml) to an output folder, closes the game,
and returns (removing the container on close).

As part of the output deliverables, it should contain these files:
.
├── camera
│ └── chess_match.m3u8
│     .. the dependent .ts files for the m3u8 ..
├── gaunt_stream
│ └── MATCH_NAME.gaunt.stream.xml
└── result.json

3 directories, 3 files

where the `result.json` file is: {"result":{"success":true | false}}.
In the case that the result of the game is NO_CONTEST, then the result should be `false`.

This container should:

- set up container environment
- allow specifying github credentials via container build argument
- allow specifying OpenRouter credentials via argument to container on startup (these may change frequently)
- build the chess game
  - C++ game
  - JS Chess Agents
- Accept a configuration JSON
  - which it transforms and passes to a script similar to `StartChessMatch` script with --appropriate-args

```json
{
  "actors": {
    "white": "<OPENROUTER_MODEL_IDENTIFIER>",
    "black": "<OPENROUTER_MODEL_IDENTIFIER>"
  },
  "scenario": {
    "enable_draw_offers": bool,
    "enable_resignations": bool,
    "enable_quips": bool
  }
}
```

## Note on commands

Although `start_chess_match.sh` passes the commands file at startup, for this container, we want to send each command
one at a time, at runtime. This will allow us to wait for the response from the command to determine how to proceed.
Will likely need to use `zmqcli` dependency to make this easy.

## Note on GUI

Containers dont render their outputs to the screen, typically. However, we need to capture the windows (and audio) created by the
Chess game. We need to figure out the best way to do this. I think we can use a virtual display server?
I want to use GPU Screen Recorder (Linux utility program) to capture the window in full resolution.

## Note on logging

I expect the container runtime to richly log everything that it does to <OUTPUT_DIRECTORY>/container_execution.log in an easy to read format.

## What the container does when invoked

1. Parse runtime arguments - THRASH_CONFIG (json) and THRASH_STREAM_ID and OPENROUTER_API_KEY
2. Initialize the chess-agents
3. Start the Chess game
   - Ensure passing the --gaunt-telemetry-xml-output-file to <OUTPUT_DIRECTORY>/gaunt_stream/<FILE> so the telemetry stream is persisted
4. Send command `configure_provider` to setup OpenRouter
5. Send command `configure_game` with `game_id` set to `THRASH_STREAM_ID`
6. Send command `open_spectator_view` with window type floating and a window size of 1920x1080
   - The result of this command should return an object containing metadata about the created window, which should include X11 window ID
   that we can use to send to GPU Screen Recorder
7. Attach the created floating window to GPU Screen Recorder and start recording
8. Send command `spectator_view::set_move_history_bar` to enable the move history bar
9. send command `start_game` to begin the game
10. every minute, poll command `query_match_result`

Successful branch:
11. When `query_match_result` returns something indicating successful match completion, wait for 1 minute.
12. End the GPU Screen Recorder - save the output to a temporary location.
13. terminate processes: Chess game, white chess-agent, black chess-agent
14. Convert the video to HLS (.m3u8 file spectator_view_default.m3u8 with accompanying .ts files) where the video is
    chunked by 2 seconds. - save this to the <OUTPUT_DIRECTORY>/camera/ dir.
15. Write <OUTPUT_DIRECTORY>/result.json with success true
16. Exit container

Unsuccessful branch:
11. When `query_match_result` returns NO_CONTEST.
12. End the GPU Screen Recorder - save the output to a temporary location.
13. terminate processes: Chess game, white chess-agent, black chess-agent
15. Write <OUTPUT_DIRECTORY>/result.json with success false
16. Exit container

## Follow these examples from a similar project

pystk2-gym.containerfile

```dockerfile
# =============================================================================
# Stage 1: Dependencies + install
# =============================================================================
FROM fedora:41 AS base

# System deps: OpenGL/Mesa (software rendering for headless), ffmpeg for video
RUN dnf install -y \
        python3 \
        python3-pip \
        python3-devel \
        g++ \
        mesa-libGL \
        mesa-libEGL \
        mesa-dri-drivers \
        libglvnd-glx \
        xorg-x11-server-Xvfb \
        ffmpeg-free \
        tree \
    && dnf clean all

# Install pystk2-gymnasium with CLI + recording extras
RUN pip install --break-system-packages --root-user-action=ignore \
        'pyzmq' \
        'numpy==1.26.4' \
        'gymnasium==1.2.3' \
        'pystk2-gymnasium[cli,record]==0.8.5'

# Pre-download SuperTuxKart game assets (~500MB) so they're cached in the image
RUN xvfb-run -a --server-args="-screen 0 1280x720x24" \
    python3 -c "import pystk2; pystk2.init(pystk2.GraphicsConfig.none())" \
    || true

# =============================================================================
# Stage 2: Final image with agent + entrypoint
# =============================================================================
FROM base AS final

ARG CACHE_BUST=0
RUN echo "CACHE_BUST = $CACHE_BUST"

WORKDIR /race

# Copy in the agent zip
COPY pystk2-gym-repeater /dreamlands/pystk2-gym-repeater
COPY server-observer-agent.zip /race/server-observer-agent.zip
COPY heuristic-agent.zip /race/heuristic-agent.zip
COPY heuristic-repeater-agent.zip /race/heuristic-repeater-agent.zip

# Output directory (mount a volume here)
RUN mkdir -p /output

# Entrypoint: run the race under Xvfb (virtual framebuffer) for headless GL,
# then copy the recording to the shared output directory.
COPY entrypoint.sh /race/entrypoint.sh
RUN chmod +x /race/entrypoint.sh

ENTRYPOINT ["/race/entrypoint.sh"]

```

entrypoint.sh

```shell
#!/bin/bash
set -euo pipefail

# Parse config
NUM_ACTORS=$(echo "$THRASH_CONFIG" | python3 -c "import sys,json; print(len(json.load(sys.stdin)['actors']))")
LAP_COUNT=$(echo "$THRASH_CONFIG" | python3 -c "import sys,json; print(json.load(sys.stdin)['scenario']['lap_count'])")
TRACK=$(echo "$THRASH_CONFIG" | python3 -c "import sys,json; print(json.load(sys.stdin)['scenario']['track'])")

# Build agent list: first is heuristic-agent, rest are heuristic-repeater-agent
AGENTS="/race/heuristic-repeater-agent.zip"
for ((i=1; i<NUM_ACTORS; i++)); do
    AGENTS="$AGENTS /race/heuristic-repeater-agent.zip"
done

echo "=== Config: ${NUM_ACTORS} actors, ${LAP_COUNT} laps, track=${TRACK} ==="

echo "=== Starting ZMQ receiver ==="
./../dreamlands/pystk2-gym-repeater \
    --gaunt-stream-id "$THRASH_STREAM_ID" \
    --live-data-endpoint tcp://*:5558 \
    --config-endpoint tcp://*:5559 \
    --configs "$THRASH_CONFIG" &
RECEIVER_PID=$!
sleep 1

echo "=== Starting headless pystk2-race ==="
xvfb-run -a --server-args="-screen 0 1280x720x24" \
    pystk2 race \
        $AGENTS \
        --num-karts "$NUM_ACTORS" \
        --laps "$LAP_COUNT" \
        --track "$TRACK" \
        --record /race/race.mp4 \
        --render-sub-steps 2 \
        --cameras 1 \
        --hide \
    || true

echo "=== Race finished, stopping ZMQ receiver ==="
python3 -c "import zmq,time; s=zmq.Context().socket(zmq.PUSH); s.connect('tcp://localhost:5558'); time.sleep(0.1); s.send_string('DONE'); s.close()"
wait "$RECEIVER_PID" 2>/dev/null || true

echo "=== Converting to HLS via Stream Copy (Lossless) ==="
if [ -f /race/race.mp4 ]; then
    mkdir -p /output/camera/race

    ffmpeg -i /race/race.mp4 \
        -codec: copy \
        -start_number 0 \
        -hls_time 6 \
        -hls_list_size 0 \
        -f hls \
        -hls_segment_filename "/output/camera/race/seg_%04d.ts" \
        /output/camera/race/race.m3u8

    echo "=== HLS Conversion Done ==="
else
    echo "ERROR: race.mp4 was not created"
    exit 1
fi

rm -f /output/result.json
echo '{"result":{"success":true}}' > /output/result.json
sync

exit 0
```

I run and build this container like:

```shell
podman build -t pystk2-race:latest -f pystk2-gym-container/pystk2-gym.containerfile --build-arg CACHE_BUST=$(date +%s)
```

```shell
podman run --name test-pystk2-race --rm \
-v $(pwd)/output:/output:Z \
-e THRASH_STREAM_ID=test_stream_id \
-e THRASH_CONFIG='{"actors":[{"name":"Tristan"},{"name":"Lonny"},{"name":"Lia"},{"name":"Erik"}],"scenario":{"lap_count":1,"track":"gran_paradiso_island"}}' \
pystk2-race:latest
```

## Building and Running (this container)

Implementation:
- `ai-chess.containerfile` — multi-stage build (Fedora 44 minimal). Clones the
  source from GitHub, builds the C++ `Chess` game and the Node chess agents, and assembles a
  slim headless runtime (Xvfb + software OpenGL, PulseAudio virtual sink, ffmpeg, node).
- `entrypoint.sh` — brings up the virtual display + audio sink, then hands off to the orchestrator.
- `orchestrate.py` — drives the game over its ZMQ command server one command at a time,
  captures the spectator window with ffmpeg (x11grab + audio), polls `query_match_result`,
  converts the capture to HLS, and writes `result.json`.

> The image builds **from the git repo**, not your local working tree. Commit and push your
> changes (including these container files and the C++ ZMQ-command-server changes) to the
> branch you build before running the build. Select it with `--build-arg GIT_REF=<branch>`.
>
> `GIT_TOKEN` needs read access to `ai-chess` **and** its private deps (`gaunt-core`,
> `nats-cxx-wrapper`), which CMake fetches over GitHub. The token is only used in the build
> stage, so it is not present in the final image.

### Build

```shell
podman build -t ai-chess:latest \
  -f extra/game-automation-container/ai-chess.containerfile \
  --build-arg GIT_TOKEN=<github_token> \
  --build-arg GIT_REF=master \
  --build-arg CACHE_BUST=$(date +%s) \
  .
```

### Run

```shell
podman run --name ai-chess --rm \
  -v $(pwd)/output:/output:Z \
  -e THRASH_STREAM_ID=test_stream \
  -e OPENROUTER_API_KEY=<openrouter_key> \
  -e THRASH_CONFIG='{"actors":{"white":"openai/gpt-4o-mini","black":"anthropic/claude-3.5-haiku"},"scenario":{"enable_draw_offers":true,"enable_resignations":true,"enable_quips":true}}' \
  ai-chess:latest
```

After a run, `output/` contains:

```
output/
├── camera/
│   ├── chess_match.m3u8
│   └── chess_match_0000.ts …
├── gaunt_stream/
│   └── test_stream.gaunt.stream.xml
├── result.json
└── container_execution.log
```
