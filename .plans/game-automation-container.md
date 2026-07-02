# Game Automation Container

A container that runs exactly one AI chess match end-to-end, headless, and writes
the match deliverables to a mounted `/output` directory. Lives under
`extra/game-automation-container/`.

## Deliverables (in the mounted `/output`)

```
output/
├── camera/
│   ├── chess_match.m3u8            # HLS playlist
│   └── chess_match_%04d.ts         # 2-second segments
├── gaunt_stream/
│   └── <STREAM_ID>.gaunt.stream.xml # Gaunt telemetry
├── result.json                     # {"result":{"success": true|false}}
├── container_execution.log         # human-readable run log
├── xvfb.log                        # Xvfb's own stderr (diagnostics)
└── pulseaudio.log                  # PulseAudio's own stderr (diagnostics)
```

`success` is `false` on `NO_CONTEST` (a voided game), `true` on any real
conclusion (`white_win`, `black_win`, `draw`).

## Runtime inputs (env vars on `podman run`)

- `THRASH_CONFIG` — JSON: `{"actors":{"white":"<model>","black":"<model>"},
  "scenario":{"enable_draw_offers":bool,"enable_resignations":bool,"enable_quips":bool}}`
- `THRASH_STREAM_ID` — used as the game id and telemetry filename.
- `OPENROUTER_API_KEY` — passed to the agents via `configure_provider`.

## Build input (build-arg)

- `GIT_TOKEN` — GitHub token with read access to `ai-chess` **and** its private
  deps (`gaunt-core`, `nats-cxx-wrapper`), which CMake FetchContent pulls. Used
  only in the build stage; not present in the final image.

## Components

- **`ai-chess.containerfile`** — multi-stage build on `quay.io/fedora/fedora-minimal:44`.
  - *Build stage*: `microdnf` toolchain + X11/wayland/vulkan/alsa `-devel`, clones
    the repo via token-HTTPS (SSH GitHub URLs rewritten with `git config --global
    --add url.…insteadOf`), builds the C++ `Chess` target with **GCC**, then builds
    the Node agents with `pnpm`.
  - *Runtime stage*: headless X (Xvfb) + software GL (mesa llvmpipe) + PulseAudio
    + ffmpeg + node + python.
- **`entrypoint.sh`** — brings up the headless environment (Xvfb, audio), then
  `exec`s the orchestrator.
- **`orchestrate.py`** — drives the game over its ZMQ command server one command at
  a time, records the spectator window, polls for completion, converts to HLS, and
  writes `result.json`.

## C++ changes made to support runtime command driving

The container needs to send commands **one at a time and read each response** (to
obtain the spectator window id and to sequence startup), whereas the existing
`start_chess_match.sh` used a fire-and-forget `--commands` file. Changes:

- `CommandFile.hpp/.cpp` — extracted a public
  `ParseCommand(type, detail, error)` from the file-loading path so the same
  type→Command dispatch is reusable at runtime.
- `Application.cpp` (ZMQ handler) — routes **all** command types (not just
  `query_match_result`) through `ParseCommand` → `ExecuteCommand`, replying with
  the executor's response correlated by `id`. Back-compat: top-level `game_id` is
  folded into `detail`.
- `ExternalCommandExecutor.hpp` — added acks (`AckReply`) to
  `configure_provider` / `configure_game` / `start_game` so a runtime caller can
  sequence on the responses instead of hanging. The startup-file path is
  unaffected (reply is null there).

---

## Build / iterate workflow (important)

The image **clones the C++/agent source from git**, but the two orchestration
scripts (`entrypoint.sh`, `orchestrate.py`) are `COPY`'d from the **local build
context** (build context = the `extra/game-automation-container` directory; they're
referenced by bare filename).

- **Editing `entrypoint.sh` / `orchestrate.py`** → plain rebuild, **no commit/push,
  no `CACHE_BUST`**. Their COPY layers rebuild on content change; everything before
  stays cached.
- **Editing the containerfile package list** → rebuild (containerfile is read
  locally), no push, no `CACHE_BUST`.
- **Editing C++ / agent code** → **commit + push**, then rebuild with
  `--build-arg CACHE_BUST=$(date +%s)` (that busts the cached `git clone`).

Run the build **from inside `extra/game-automation-container`** (context `.`):

```shell
podman build -t ai-chess:latest -f ai-chess.containerfile \
  --build-arg GIT_TOKEN=<token> --build-arg GIT_REF=<branch> .
```

There's also a bind-mount debug loop (skip rebuild entirely for the scripts):
add `-v $(pwd)/entrypoint.sh:/app/entrypoint.sh:Z -v $(pwd)/orchestrate.py:/app/orchestrate.py:Z`
to `podman run`.

---

## Bring-up problems fixed along the way

### Build-stage issues
- **Headless FPS limiter → MangoHud** → the game is capped with **MangoHud**
  (`mangohud` from Fedora's base repos), run as `mangohud /app/Chess …` with
  `MANGOHUD_CONFIG=fps_limit=<n>,no_display=1` (see `launch_game()` in
  `orchestrate.py`). This replaced **libstrangle**, which died at launch on
  Fedora 44 (glibc 2.41) with `/app/Chess: symbol lookup error: … undefined symbol:
  __libc_dlsym` — libstrangle interposes `dlsym` and reached the genuine one via the
  glibc-private `__libc_dlsym`, dropped from the dynamic symbol table in glibc 2.34+.
  libstrangle is abandoned and no prebuilt package (incl. the atim Copr) carries a
  fix; MangoHud is actively maintained and has no such issue. `no_display=1` keeps
  the overlay out of the capture. (The atim Copr was tried first and failed
  identically at runtime — its build is unpatched upstream.)
- **libxcrypt vcpkg port** needed `autoconf-archive` → added to build deps.
- **`nats-cxx-wrapper` clone "Host key verification failed"** → two `git config
  insteadOf` calls with the same key; the second overwrote the first. Fixed with
  `--add` so both the SCP (`git@github.com:`) and `ssh://` forms rewrite to token
  HTTPS.
- **`BoardAtlas.hpp` consteval step-limit** → the consteval scan of the embedded
  `Board.rgba` blows GCC's default constexpr op limit under libstdc++ 16 headers.
  Fixed with `-DCMAKE_CXX_FLAGS="-fconstexpr-ops-limit=2000000000"`. (User directive:
  **use GCC, not Clang.**)
- **`pnpm install --frozen-lockfile`** → `pnpm-lock.yaml` is gitignored, absent from
  the clone. Changed to `--no-frozen-lockfile`.
- **`xorg-x11-utils` "No match"** on Fedora 44 (package dropped) → removed it; it was
  only pulled in for `xdpyinfo`.

### Runtime bring-up: the container exited silently
- **Symptom**: container exited right after "Starting Xvfb" with no success/error
  line. **Cause**: `exec > >(tee -a "$LOG_FILE")` process substitution swallowed the
  last buffered lines on exit, hiding the real failure.
- **Fixes** in `entrypoint.sh`:
  - Line-buffered logging: `exec > >(stdbuf -oL -eL tee -a "$LOG_FILE") 2>&1`.
  - `trap … EXIT` records the exit code; `trap … ERR` prints the failing command +
    line number.
  - Truncate the log at start (`: > "$LOG_FILE"`) so each run is fresh (the volume
    persists across runs).
  - Xvfb readiness now keys on the **X socket** (`/tmp/.X11-unix/X99`) + process-alive
    check, and Xvfb's stderr goes to `xvfb.log`. Pre-create `/tmp/.X11-unix` (1777).
  - `xdotool` connectivity validated separately with its real error surfaced (it's
    what the orchestrator uses to move/raise the window).

---

## Audio (the harder of the two A/V concerns)

### Concern 1 — PulseAudio `--system` mode was broken and noisy
Running `pulseaudio --system` dropped privileges to the `pulse` user, which then
couldn't bind our socket under `/tmp` (`bind(): Permission denied`) and had no D-Bus
system bus — producing a wall of `E:`/`W:` lines and failing to create the sink.

**Fix**: run PulseAudio as a **plain per-user daemon** (we're root) with `-n`
(skip the stock config, so no D-Bus/udev module autospawn), loading exactly:
`module-native-protocol-unix` (socket `/tmp/pulse/native`, `auth-anonymous=1`),
`module-null-sink sink_name=chess`, `module-always-sink`. Its stdout/stderr are
redirected to `output/pulseaudio.log` so any residual warnings never pollute
`container_execution.log`. `XDG_RUNTIME_DIR=/tmp/xdg` and `HOME` are set so the
daemon has a place for its runtime/cookie.

### Concern 2 — SDL3 has no PulseAudio backend
Even with Pulse healthy, the game logged:
`Audio unavailable (Audio target 'pulse' not available); … Drivers: alsa disk dummy`.
The statically-linked SDL3 in the `Chess` binary was built (via the vcpkg sdl3 port)
with the **alsa** feature but **not** pulseaudio — so `SDL_AUDIODRIVER=pulse` can
never work, and setting it explicitly made SDL fail hard instead of falling back.

**Fix (runtime-only ALSA→Pulse bridge)** — chosen over rebuilding SDL because it
needs no push and no long C++ rebuild:
- Added `alsa-plugins-pulseaudio` to the runtime image.
- `entrypoint.sh` writes `/etc/asound.conf` making ALSA's default PCM/CTL the
  `pulse` plugin pointed at `unix:/tmp/pulse/native`.
- Set `SDL_AUDIODRIVER=alsa`.
- Signal path: game → SDL(alsa) → ALSA `default` → alsa-pulse plugin → Pulse
  `chess` sink → `chess.monitor` → ffmpeg (`-f pulse -i chess.monitor`, `-c:a aac`).

`orchestrate.py` gates audio on `audio_available()` (checks `pactl list short
sources` for `chess.monitor`) and records video-only if it's absent, so audio stays
strictly best-effort.

> **Status at handoff**: the ALSA-bridge change was made but not yet verified by a
> run. The next run should show the `Audio unavailable` line gone and
> `Recording with audio=True`. If ALSA still complains, the messages appear inline
> in `container_execution.log` (they come from the game process).

---

## Video

### Concern — `Unknown encoder 'libx264'` (fatal)
Fedora's `ffmpeg-free` is patent-stripped and ships **without libx264**, so the
hardcoded `-c:v libx264` recording command failed and produced **no video at all**.

**Fix**: `orchestrate.py` now **probes `ffmpeg -encoders`** and picks the best
available H.264-capable encoder, cached after first call:
`libx264` → `libopenh264` (real H.264) → `h264_vaapi` → `mpeg2video` (always
available, valid in MPEG-TS/HLS) as last resort. Per-encoder flag sets are applied
(`video_encoder_args`) because they don't share option names (e.g. libopenh264 has
no `-preset`; it's driven by `-b:v`). The choice is logged as
`Selected video encoder: <name>`. Both the live recording and the HLS re-encode
fallback use the picker.

Recording still forces keyframes every 2s
(`-force_key_frames expr:gte(t,n_forced*2)`) so the capture can be stream-copied
into aligned 2-second HLS segments; `convert_to_hls()` tries `-c copy` first and
falls back to a re-encode.

> **Status at handoff**: need to read the `Selected video encoder:` line from a run.
> If it's `mpeg2video`, HLS will work but browser players (hls.js) prefer H.264 —
> add the `openh264` package to the runtime image to get `libopenh264`.

---

## Known gaps / future improvements

- **Confirm audio end-to-end.** Verify `Recording with audio=True` and that the
  produced `.ts` segments actually carry an audio track. If the ALSA bridge proves
  flaky, the more robust fix is to **rebuild SDL3 with the pulseaudio backend**
  (add the pulseaudio feature to the vcpkg sdl3 port) and drive Pulse directly.
- **Real H.264.** If the encoder probe selects `mpeg2video`, add `openh264` to the
  runtime image so `libopenh264` is available; confirm HLS plays in the target
  player.
- **Spectator window targeting.** `open_spectator_view` currently returns
  `x11_id=None`, so the recorder grabs the whole framebuffer instead of tracking the
  window. It works because the floating spectator window is created at (0,0) filling
  1920×1080, but the C++ side should populate the X11 XID
  (`window_metadata.os_metadata.detail.window_id`) so `xdotool` can move/raise/size
  the exact window — more robust if the layout ever changes.
- **Match-length safety.** `MAX_MATCH_SECONDS` (2h) caps a hung game; consider a
  tighter, configurable cap and clearer `timeout` handling in `result.json`.
- **Segment/playlist naming.** Deliverable tree names the playlist
  `chess_match.m3u8`; confirm this matches whatever consumes the output downstream.
- **Image slimming.** Runtime deps were trimmed but not audited; a pass to drop
  anything unused (and to pin versions) would shrink the image and improve
  reproducibility.
- **Result granularity.** `result.json` is currently just `success` bool; consider
  including outcome/winner/cause for downstream consumers if useful.
