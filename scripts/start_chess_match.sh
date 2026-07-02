#!/usr/bin/env bash

# start_chess_match.sh - Coordinate an AI Chess Match
#
# This script launches two instances of the Chess Agent client (for White and Black players)
# in the background, waits for them to initialize, and then runs the Chess server in the 
# foreground. When the Chess server exits, or if this script is terminated (e.g. by Ctrl+C),
# all background agent client processes are automatically shut down.
#
# Usage:
#   ./scripts/start_chess_match.sh \
#       --white-port <PORT> \
#       --white-model <MODEL> \
#       --black-port <PORT> \
#       --black-model <MODEL> \
#       --retrospective-rounds <INT> \
#       --openrouter-api-key <KEY> \
#       --chess-server <FILEPATH_TO_SERVER> \
#       --chess-agent-client <FILEPATH_TO_AGENT>
#
# Options:
#   --white-port <PORT>                 ZMQ port for the White Agent (e.g. 5555)
#   --white-model <MODEL>               OpenRouter model name for the White Agent
#   --black-port <PORT>                 ZMQ port for the Black Agent (e.g. 5556)
#   --black-model <MODEL>               OpenRouter model name for the Black Agent
#   --retrospective-rounds <INT>        Number of retrospective rounds for agents (optional, default: 0)
#   --openrouter-api-key <KEY>          OpenRouter API key
#   --chess-server <FILEPATH>           Path to the Chess C++ server executable
#   --chess-agent-client <FILEPATH>     Path to the agent client index.ts or dist/index.js
#   -h, --help                          Show this help message

# Shell safety configuration
set -euo pipefail

# Argument defaults
WHITE_PORT=""
WHITE_MODEL=""
BLACK_PORT=""
BLACK_MODEL=""
RETROSPECTIVE_ROUNDS=0
OPENROUTER_API_KEY=""
CHESS_SERVER=""
CHESS_AGENT_CLIENT=""
GAUNT_TELEMETRY_XML_OUTPUT_FILE=""
COMMANDS_FILE=""

# Runtime ZMQ command server port (same as the container's orchestrate.py). The
# game::estimate_cost poller below talks to this; requires python3 + pyzmq.
COMMAND_SERVER_PORT=5599

# Print usage instructions
print_usage() {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  --white-port <PORT>                 ZMQ port for White Agent"
    echo "  --white-model <MODEL>               OpenRouter model name for White Agent"
    echo "  --black-port <PORT>                 ZMQ port for Black Agent"
    echo "  --black-model <MODEL>               OpenRouter model name for Black Agent"
    echo "  --retrospective-rounds <INT>        Number of retrospective rounds for agents (optional, default: 0)"
    echo "  --openrouter-api-key <KEY>          OpenRouter API key"
    echo "  --chess-server <FILEPATH>           Path to the Chess server executable"
    echo "  --chess-agent-client <FILEPATH>     Path to the chess-agent client (TypeScript or JavaScript)"
    echo "  --gaunt-telemetry-xml-output-file <PATH>  Write Gaunt telemetry XML stream to this file (optional)"
    echo "  -h, --help                          Show this help message"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --white-port)
            WHITE_PORT="$2"
            shift 2
            ;;
        --white-model)
            WHITE_MODEL="$2"
            shift 2
            ;;
        --black-port)
            BLACK_PORT="$2"
            shift 2
            ;;
        --black-model)
            BLACK_MODEL="$2"
            shift 2
            ;;
        --retrospective-rounds)
            RETROSPECTIVE_ROUNDS="$2"
            shift 2
            ;;
        --openrouter-api-key)
            OPENROUTER_API_KEY="$2"
            shift 2
            ;;
        --chess-server)
            CHESS_SERVER="$2"
            shift 2
            ;;
        --chess-agent-client)
            CHESS_AGENT_CLIENT="$2"
            shift 2
            ;;
        --gaunt-telemetry-xml-output-file)
            GAUNT_TELEMETRY_XML_OUTPUT_FILE="$2"
            shift 2
            ;;
        -h|--help)
            print_usage
            exit 0
            ;;
        *)
            echo "Error: Unknown argument '$1'" >&2
            print_usage >&2
            exit 1
            ;;
    esac
done

# Validate required inputs
missing_args=()
[[ -z "$WHITE_PORT" ]] && missing_args+=("--white-port")
[[ -z "$WHITE_MODEL" ]] && missing_args+=("--white-model")
[[ -z "$BLACK_PORT" ]] && missing_args+=("--black-port")
[[ -z "$BLACK_MODEL" ]] && missing_args+=("--black-model")
[[ -z "$OPENROUTER_API_KEY" ]] && missing_args+=("--openrouter-api-key")
[[ -z "$CHESS_SERVER" ]] && missing_args+=("--chess-server")
[[ -z "$CHESS_AGENT_CLIENT" ]] && missing_args+=("--chess-agent-client")

if [ ${#missing_args[@]} -ne 0 ]; then
    echo "Error: Missing required arguments: ${missing_args[*]}" >&2
    print_usage >&2
    exit 1
fi

# Ensure files exist
if [ ! -f "$CHESS_SERVER" ]; then
    echo "Error: Chess server executable not found at '$CHESS_SERVER'" >&2
    exit 1
fi
if [ ! -f "$CHESS_AGENT_CLIENT" ]; then
    echo "Error: Chess agent client file not found at '$CHESS_AGENT_CLIENT'" >&2
    exit 1
fi

# Store spawned process PIDs for termination handling
AGENT_PIDS=()
# PID of the background game::estimate_cost poller (empty until started).
COST_POLLER_PID=""

# Query the runtime game::estimate_cost command once and print the JSON reply.
# Mirrors orchestrate.py's CommandClient: a DEALER that sends {type,id,detail}
# and reads one reply frame. Port and game id are passed as argv.
query_estimated_game_cost() {
    python3 - "$COMMAND_SERVER_PORT" "$GAME_ID" <<'PYEOF'
import sys, uuid, zmq
port, game_id = sys.argv[1], sys.argv[2]
sock = zmq.Context.instance().socket(zmq.DEALER)
sock.setsockopt(zmq.LINGER, 0)
sock.connect(f"tcp://127.0.0.1:{port}")
sock.send_json({"type": "game::estimate_cost", "id": uuid.uuid4().hex,
                "detail": {"game_id": game_id}})
print(sock.recv_json() if sock.poll(5000) else {"error": "timeout"})
PYEOF
}

# Process cleanup handler
cleanup() {
    local exit_status=$?
    echo -e "\n[start_chess_match] Script exiting. Cleaning up background agent processes..."
    if [[ -n "$COST_POLLER_PID" ]] && kill -0 "$COST_POLLER_PID" 2>/dev/null; then
        echo "[start_chess_match] Stopping estimate_cost poller PID $COST_POLLER_PID..."
        kill "$COST_POLLER_PID" 2>/dev/null || true
        wait "$COST_POLLER_PID" 2>/dev/null || true
    fi
    for pid in "${AGENT_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            echo "[start_chess_match] Terminating agent PID $pid..."
            kill "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
        fi
    done
    if [[ -n "$COMMANDS_FILE" && -f "$COMMANDS_FILE" ]]; then
        rm -f "$COMMANDS_FILE"
    fi
    exit "$exit_status"
}

# Trap exit signals for proper cleanup of background jobs
trap cleanup EXIT INT TERM

# Start White Agent (it binds its port and waits for the setup command, which
# now carries provider/model/credentials).
echo "[start_chess_match] Starting White Agent client (waiting for setup)..."
if [[ "$CHESS_AGENT_CLIENT" == *.ts ]]; then
    npx ts-node "$CHESS_AGENT_CLIENT" \
        --name "White Agent" \
        --endpoint "tcp://127.0.0.1:$WHITE_PORT" &
    AGENT_PIDS+=($!)
else
    node "$CHESS_AGENT_CLIENT" \
        --name "White Agent" \
        --endpoint "tcp://127.0.0.1:$WHITE_PORT" &
    AGENT_PIDS+=($!)
fi

# Start Black Agent
echo "[start_chess_match] Starting Black Agent client (waiting for setup)..."
if [[ "$CHESS_AGENT_CLIENT" == *.ts ]]; then
    npx ts-node "$CHESS_AGENT_CLIENT" \
        --name "Black Agent" \
        --endpoint "tcp://127.0.0.1:$BLACK_PORT" &
    AGENT_PIDS+=($!)
else
    node "$CHESS_AGENT_CLIENT" \
        --name "Black Agent" \
        --endpoint "tcp://127.0.0.1:$BLACK_PORT" &
    AGENT_PIDS+=($!)
fi

# Briefly wait for background agents to spin up
sleep 1.5

# Generate a startup command file: configure the OpenRouter provider, configure
# the game (per-color provider/model/endpoint), then start it. The Chess app
# runs these sequentially at startup.
GAME_ID="match"
COMMANDS_FILE="$(mktemp /tmp/chess-commands.XXXXXX.json)"
cat > "$COMMANDS_FILE" <<EOF
{
  "commands": [
    {
      "type": "configure_provider",
      "detail": { "name": "openrouter", "kind": "OpenRouter", "api_key": "$OPENROUTER_API_KEY" }
    },
    {
      "type": "configure_game",
      "detail": {
        "game_id": "$GAME_ID",
        "white": { "endpoint": "tcp://127.0.0.1:$WHITE_PORT", "provider": "OpenRouter", "model_id": "$WHITE_MODEL" },
        "black": { "endpoint": "tcp://127.0.0.1:$BLACK_PORT", "provider": "OpenRouter", "model_id": "$BLACK_MODEL" },
        "enable_quip": true,
        "enable_draw_offer": true,
        "enable_resignation": true,
        "retrospective_turn_count": $RETROSPECTIVE_ROUNDS
      }
    },
    {
      "type": "open_spectator_view",
      "detail": {
        "game_id": "$GAME_ID",
        "game_view_theme": "default",
        "window_configuration": {
          "type": "floating",
          "window_size": {
            "width": 1920,
            "height": 1080
          },
          "window_position": {
            "x": 0,
            "y": 0
          },
          "imgui_flags": [
            "ImGuiWindowFlags_NoTitleBar"
          ]
        }
      }
    },
    {
      "type": "spectator_view::set_move_history_bar",
      "detail": {
        "window_id": "Spectator 1 - $GAME_ID",
        "enable_move_history_bar": true
      }
    },
    {
      "type": "start_game",
      "detail": { "game_id": "$GAME_ID" }
    }
  ]
}
EOF

# Run Chess in the foreground, driving configuration/start from the command file.
echo "[start_chess_match] Launching Chess with command file $COMMANDS_FILE..."
chmod +x "$CHESS_SERVER"

# Assemble the Chess server arguments, appending the optional telemetry flag.
# The command-server endpoint enables the runtime ZMQ command interface that the
# estimate_cost poller (below) queries.
CHESS_SERVER_ARGS=(--commands "$COMMANDS_FILE" --command-server-endpoint "tcp://127.0.0.1:$COMMAND_SERVER_PORT")
if [[ -n "$GAUNT_TELEMETRY_XML_OUTPUT_FILE" ]]; then
    CHESS_SERVER_ARGS+=(--gaunt-telemetry-xml-output-file "$GAUNT_TELEMETRY_XML_OUTPUT_FILE")
fi

# Periodically exercise game::estimate_cost (every 2 min) so we can confirm the
# command is answering while the match runs. Runs until the script exits; the
# initial wait also lets the command server come up and the game start first.
estimate_cost_poller() {
    while true; do
        sleep 120
        echo "[start_chess_match] game::estimate_cost ->"
        query_estimated_game_cost || echo "[start_chess_match] estimate_cost query failed"
    done
}
estimate_cost_poller &
COST_POLLER_PID=$!

"$CHESS_SERVER" "${CHESS_SERVER_ARGS[@]}"
