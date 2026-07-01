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
# clang is required for the project's C++26 `#embed` / `--embed-dir` usage; gcc
# is kept because vcpkg builds its ports with the system compiler. The X11 /
# wayland / vulkan / alsa -devel packages satisfy vcpkg's sdl3 port (built with
# features: x11 wayland vulkan alsa). ImageMagick + ffmpeg are used at CMake
# configure time to bake texture atlases and placeholder WAVs.
RUN microdnf install -y --setopt=install_weak_deps=0 \
        clang lld llvm \
        gcc gcc-c++ \
        cmake ninja-build make \
        git ca-certificates \
        perl pkgconf-pkg-config which \
        zip unzip tar xz bzip2 \
        autoconf automake libtool \
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

RUN git config --global "url.https://x-access-token:${GIT_TOKEN}@github.com/.insteadOf" "git@github.com:" \
    && git config --global "url.https://x-access-token:${GIT_TOKEN}@github.com/.insteadOf" "ssh://git@github.com/"

WORKDIR /src
RUN GIT_REPO_WITH_TOKEN="$(echo "${GIT_REPO_URL}" | sed -E "s#https://github.com/#https://x-access-token:${GIT_TOKEN}@github.com/#")" \
    && git clone --branch "${GIT_REF}" "${GIT_REPO_WITH_TOKEN}" ai-chess

WORKDIR /src/ai-chess

# --- Build the C++ game ------------------------------------------------------
# vcpkg (bootstrapped automatically by the cool-vcpkg FetchContent module) builds
# SDL3/imgui/zmq/cpr/spdlog/etc. from source — this stage is long. The main
# target is compiled with clang for the #embed support.
RUN cmake -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_COMPILER=clang++ \
    && cmake --build build --target Chess

# --- Build the JS chess agents ----------------------------------------------
WORKDIR /src/ai-chess/chess-agent
RUN pnpm install --frozen-lockfile && pnpm build

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
        alsa-lib pulseaudio pulseaudio-utils \
        ffmpeg-free \
        xdotool xorg-x11-utils \
        nodejs \
        python3 python3-pyzmq \
        ca-certificates procps-ng tree \
    && microdnf clean all

WORKDIR /app

# Artifacts from the build stage (everything originates from the cloned repo).
COPY --from=build /src/ai-chess/build/Chess /app/Chess
COPY --from=build /src/ai-chess/chess-agent /app/chess-agent
COPY --from=build /src/ai-chess/extra/game-automation-container/entrypoint.sh /app/entrypoint.sh
COPY --from=build /src/ai-chess/extra/game-automation-container/orchestrate.py /app/orchestrate.py

RUN chmod +x /app/Chess /app/entrypoint.sh

# Mount a volume here to collect the deliverables.
RUN mkdir -p /output

ENTRYPOINT ["/app/entrypoint.sh"]
