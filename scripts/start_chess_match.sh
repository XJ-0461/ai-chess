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

# Process cleanup handler
cleanup() {
    local exit_status=$?
    echo -e "\n[start_chess_match] Script exiting. Cleaning up background agent processes..."
    for pid in "${AGENT_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            echo "[start_chess_match] Terminating agent PID $pid..."
            kill "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
        fi
    done
    exit "$exit_status"
}

# Trap exit signals for proper cleanup of background jobs
trap cleanup EXIT INT TERM

# Start White Agent
echo "[start_chess_match] Starting White Agent client..."
if [[ "$CHESS_AGENT_CLIENT" == *.ts ]]; then
    npx ts-node "$CHESS_AGENT_CLIENT" \
        --model "$WHITE_MODEL" \
        --name "White Agent" \
        --endpoint "tcp://127.0.0.1:$WHITE_PORT" \
        --openrouter-api-key "$OPENROUTER_API_KEY" &
    AGENT_PIDS+=($!)
else
    node "$CHESS_AGENT_CLIENT" \
        --model "$WHITE_MODEL" \
        --name "White Agent" \
        --endpoint "tcp://127.0.0.1:$WHITE_PORT" \
        --openrouter-api-key "$OPENROUTER_API_KEY" &
    AGENT_PIDS+=($!)
fi

# Start Black Agent
echo "[start_chess_match] Starting Black Agent client..."
if [[ "$CHESS_AGENT_CLIENT" == *.ts ]]; then
    npx ts-node "$CHESS_AGENT_CLIENT" \
        --model "$BLACK_MODEL" \
        --name "Black Agent" \
        --endpoint "tcp://127.0.0.1:$BLACK_PORT" \
        --openrouter-api-key "$OPENROUTER_API_KEY" &
    AGENT_PIDS+=($!)
else
    node "$CHESS_AGENT_CLIENT" \
        --model "$BLACK_MODEL" \
        --name "Black Agent" \
        --endpoint "tcp://127.0.0.1:$BLACK_PORT" \
        --openrouter-api-key "$OPENROUTER_API_KEY" &
    AGENT_PIDS+=($!)
fi

# Briefly wait for background agents to spin up
sleep 1.5

# Run Chess server in the foreground
echo "[start_chess_match] Launching Chess Server..."
chmod +x "$CHESS_SERVER"
"$CHESS_SERVER" --retrospective-rounds $RETROSPECTIVE_ROUNDS --white-endpoint "tcp://127.0.0.1:$WHITE_PORT" --black-endpoint "tcp://127.0.0.1:$BLACK_PORT"
