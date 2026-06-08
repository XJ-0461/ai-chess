# AI Chess Game Flow

This is a chess game where AI Agents play against each other.

The Agent Client will be handled by a lightweight typescript service that communicates with the Chess server (C++ service) and the OpenRouter API to facilitate moves, using ZeroMQ for IPC between the agent client and chess server.

## Setup

### Start Agent Clients

node chess-agent --model <OPENROUTER_MODEL_NAME> --name <AGENT_NAME> --endpoint <PORT_OR_URI> --openrouter-api-key <OPENROUTER_API_KEY>

### Start Chess Game Server

./Chess --white-endpoint <WHITE_ENDPOINT> --black-endpoint <BLACK_ENDPOINT>

## Chess Game Server Flow

1. Chess Game Server starts and sends PING to WHITE client.
2. On PONG response, server sends PING to BLACK client.
3. Send initialization message to each client
    - { type: setup, color: WHITE | BLACK }
4. When both clients respond, send the first move request to the WHITE client
5. Game Loop
    - WHITE sends move decision { type: move_decision, algebraic_move_string: algebraic_move_string }
    - Server validates move
        - If invalid, send error message back with error code
        - If valid, update board state and send game_history message to BLACK client
    - BLACK sends move decision { type: move_decision, algebraic_move_string: algebraic_move_string }
    - Server validates move
        - If invalid, send error message back with error code
        - If valid, update board state and send game_history message to WHITE client
    - ... Repeat until checkmate, stalemate, or draw
6. Send END message to both clients

## Game Chess Server Details

The Chess application will follow a layout that features three distinct areas:
- left hand side panel (White player details)
    - Top to bottom
        - Header (Player name/ model name)
        - Agent Chat (reasoning and responses)
- main game area (chess GUI)
- right hand side panel (Black player details)
    - Top to bottom
        - Header (Player name/ model name)
        - Agent Chat (reasoning and responses)

We have integrated the Agent Chat UI elements from the `.reference/Kolosal` project into the main project. The implementation details are as follows:
- Located in [AgentSidebar.h](file:///home/tristan/Programs/ai-chess/ai-chess/src/Application/AgentChat/AgentSidebar.h) and [AgentSidebar.cpp](file:///home/tristan/Programs/ai-chess/ai-chess/src/Application/AgentChat/AgentSidebar.cpp).
- Features a header panel for each side displaying:
  - Color-coded connection/thinking status dot (Gray = Disconnected, Green = Connected/Ready, Yellow = Thinking).
  - Agent Name and active LLM Model.
  - Active status text.
- History log showing past moves, timestamps, and collapsible "Thoughts" (reasoning block, uncollapsed/expanded by default for new turns), commentary response, and play decisions.
- Auto-scroll (enabled by default) and manual Clear controls in the footer.
- Embedded in the main application dockspace as default floating panels on the left/right window margins.

Will use ZeroMQ (ZMQ) PAIR sockets to facilitate real-time communication between the Chess application and the Agent clients.

## Agent Client Flow

1. Receive initialization message
2. Initialize OpenRouter session with program arguments
3. Receive move message
    - Send request to OpenRouter Agent SDK.
        - Stream reasoning, response, and move messages back to the Chess server in real time.
        - Call the `make_move` tool to submit the move decision.
4. Send move message back with algebraic move string

## Agent Client Details

Use ZeroMQ to communicate with the Chess server.
Use OpenRouter Agent SDK to facilitate requests to the LLMs.
When move message is received:
    - make request to openrouter
    - stream reasoning and responses back to the server as they are returned from the model.
    - return the move decision back to the server as it is returned from the model.

## Message Types

Json format

Ping:
    { type: ping }
Pong:
    { type: pong }
Setup:
    { type: setup, color: WHITE | BLACK }
SetupAck:
    { type: setup_ack, color: WHITE | BLACK }
StartMove:
    { type: start_move, color: WHITE | BLACK }
GameHistory:
    { type: game_history, game_history: [ algebraic_move_string, algebraic_move_string, ...] }
Error:
    { type: ERROR, level: ERROR | CRITICAL, code: ERROR_CODE }
ReasoningMessage:
    { type: reasoning, message: string }
ResponseMessage:
    { type: response, message: string }
MoveDecision:
    { type: move_decision, algebraic_move_string: string }
EndGame:
    { type: end_game, winner: WHITE | BLACK | DRAW, cause: CHECKMATE | STALEMATE | DRAW }


