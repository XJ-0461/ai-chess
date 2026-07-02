# =============================================================================
# Chess Game Automation Container
#
# Runs exactly one AI chess match end-to-end, headless, and writes the match
# deliverables (HLS video, Gaunt telemetry XML, result.json) to /output.
#
# The source is cloned from GitHub at build time (GIT_TOKEN build-arg). The C++
# `Chess` game and the two Node `chess-agent` processes are built here; a slim
# runtime stage then carries only the artifacts + headless X/audio/ffmpeg stack.
# =============================================================================

# =============================================================================
# Stage 1: Build — clone source, build the C++ game and the JS agents
# =============================================================================
FROM quay.io/fedora/fedora-minimal:44 AS build

# --- Toolchain + build dependencies -----------------------------------------
# gcc (15+) builds both the project (it supports C++26 `#embed` / --embed-dir) and
# vcpkg's ports. The X11 / wayland / vulkan / alsa -devel packages satisfy vcpkg's
# sdl3 port (built with features: x11 wayland vulkan alsa). ImageMagick + ffmpeg
# are used at CMake configure time to bake texture atlases and placeholder WAVs.
RUN microdnf install -y --setopt=install_weak_deps=0 \
        gcc gcc-c++ \
        cmake ninja-build make \
        git ca-certificates \
        perl pkgconf-pkg-config which \
        zip unzip tar xz bzip2 \
        autoconf autoconf-archive automake libtool \
        python3 \
        nodejs npm \
        ImageMagick ffmpeg-free \
        libX11-devel libXext-devel libXcursor-devel libXi-devel \
        libXrandr-devel libXfixes-devel libXScrnSaver-devel libXtst-devel \
        libxkbcommon-devel libxkbcommon-x11-devel \
        wayland-devel wayland-protocols-devel libdrm-devel mesa-libgbm-devel \
        vulkan-loader-devel vulkan-headers \
        alsa-lib-devel \
        mesa-libGL-devel mesa-libEGL-devel \
    && microdnf clean all

RUN npm install -g pnpm@10.24.0

# --- Source acquisition ------------------------------------------------------
# GIT_TOKEN: a GitHub token (PAT or app installation token) with read access to
# the ai-chess repo AND its private sibling deps (gaunt-core, nats-cxx-wrapper),
# which CMake FetchContent pulls over git@github.com. We rewrite SSH GitHub URLs
# to token-authenticated HTTPS so those private fetches succeed non-interactively.
ARG GIT_TOKEN
ARG GIT_REPO_URL=https://github.com/XJ-0461/ai-chess.git
ARG GIT_REF=main
# Bump to force a fresh clone + rebuild (defeats layer caching), e.g. $(date +%s).
ARG CACHE_BUST=0
RUN echo "CACHE_BUST=${CACHE_BUST}"

# insteadOf is multi-valued: use --add so BOTH the SCP form (git@github.com:owner/repo)
# and the ssh:// form are rewritten to token HTTPS. (Without --add, the second config
# call would overwrite the first, leaving the SCP form — used by the gaunt/nats
# FetchContent deps — to fall back to SSH and fail with "Host key verification failed".)
RUN git config --global url."https://x-access-token:${GIT_TOKEN}@github.com/".insteadOf "git@github.com:" \
    && git config --global --add url."https://x-access-token:${GIT_TOKEN}@github.com/".insteadOf "ssh://git@github.com/"

WORKDIR /src
RUN GIT_REPO_WITH_TOKEN="$(echo "${GIT_REPO_URL}" | sed -E "s#https://github.com/#https://x-access-token:${GIT_TOKEN}@github.com/#")" \
    && git clone --branch "${GIT_REF}" "${GIT_REPO_WITH_TOKEN}" ai-chess

WORKDIR /src/ai-chess

# --- Build the C++ game ------------------------------------------------------
# vcpkg (bootstrapped automatically by the cool-vcpkg FetchContent module) builds
# SDL3/imgui/zmq/cpr/spdlog/etc. from source — this stage is long. The main
# target is compiled with gcc (gcc 15+ supports #embed / --embed-dir).
#
# -fconstexpr-ops-limit: BoardAtlas.hpp scans the embedded ~80KB Board.rgba in a
# consteval function; with Fedora's libstdc++ 16 headers (which run bounds-check
# preconditions during constant evaluation) this blows GCC's default constexpr
# operation limit (~33M). Raise it well above what the scan needs.
#
# Pre-seed the util-linux tarball for vcpkg's libmount port: its kernel.org
# download URL now 404s, so fetch the identical tarball from a live mirror into
# vcpkg's downloads cache. vcpkg finds it by filename, validates the SHA512, and
# skips the dead URL. (python3 is already installed; avoids adding curl/wget.)
RUN mkdir -p build/external/cool-vcpkg/downloads \
    && python3 -c "import urllib.request; urllib.request.urlretrieve('https://download.nus.edu.sg/mirror/gentoo/distfiles/8f/util-linux-2.41.3.tar.xz', 'build/external/cool-vcpkg/downloads/util-linux-2.41.3.tar.xz')"

RUN cmake -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=gcc \
        -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_CXX_FLAGS="-fconstexpr-ops-limit=2000000000" \
    && cmake --build build --target Chess

# --- Build the JS chess agents ----------------------------------------------
WORKDIR /src/ai-chess/chess-agent
# pnpm-lock.yaml is gitignored (not in the clone), so resolve fresh rather than
# requiring a frozen lockfile.
RUN pnpm install --no-frozen-lockfile && pnpm build

# =============================================================================
# Stage 2: Runtime — headless X + software GL + audio + ffmpeg + node
# =============================================================================
FROM quay.io/fedora/fedora-minimal:44 AS runtime

# The Chess binary links its libraries statically; only system X11/GL/audio libs
# are dynamic (SDL3 also dlopens several of these at runtime). Xvfb provides the
# virtual display, mesa-dri-drivers/llvmpipe the software OpenGL, PulseAudio the
# virtual audio sink, and ffmpeg the x11grab capture + HLS conversion.
RUN microdnf install -y --setopt=install_weak_deps=0 \
        xorg-x11-server-Xvfb \
        mesa-dri-drivers mesa-libGL mesa-libEGL \
        libglvnd-glx libglvnd-opengl libglvnd-egl \
        libX11 libXext libXcursor libXi libXrandr libXfixes \
        libXScrnSaver libXtst libxkbcommon libxkbcommon-x11 \
        libwayland-client libwayland-egl \
        vulkan-loader \
        alsa-lib alsa-plugins-pulseaudio pulseaudio pulseaudio-utils \
        ffmpeg-free \
        xdotool \
        nodejs \
        python3 python3-pyzmq \
        mangohud \
        ca-certificates procps-ng tree \
    && microdnf clean all

# mangohud (headless FPS limiter): the render loop's only throttle is vsync, a
# no-op under Xvfb + llvmpipe, so it free-runs and the software rasterizer pins
# every core. MangoHud's OpenGL layer caps the buffer-swap rate via its fps_limit
# (see launch_game() in orchestrate.py, which runs the game under `mangohud` with
# MANGOHUD_CONFIG=fps_limit=<n>,no_display=1). It's in Fedora's base repos and
# actively maintained, unlike libstrangle (whose glibc-private __libc_dlsym usage
# no longer resolves on glibc 2.34+).

WORKDIR /app

# Compiled artifacts come from the build stage (they originate from the cloned repo).
COPY --from=build /src/ai-chess/build/Chess /app/Chess
COPY --from=build /src/ai-chess/chess-agent /app/chess-agent
# The orchestration scripts are copied from the local build context (your working
# tree), NOT the git clone. This lets you iterate on them with a plain rebuild —
# editing either file invalidates only its COPY layer (no commit/push, no
# CACHE_BUST needed). The build context is this directory (extra/game-automation-container),
# so these are referenced by bare filename (see the build command in readme.md).
COPY entrypoint.sh /app/entrypoint.sh
COPY orchestrate.py /app/orchestrate.py

RUN chmod +x /app/Chess /app/entrypoint.sh

# Mount a volume here to collect the deliverables.
RUN mkdir -p /output

ENTRYPOINT ["/app/entrypoint.sh"]
