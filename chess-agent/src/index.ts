import { Command } from "commander";
import { ZmqClient } from "./zmq-client";
import { OpenRouterClient } from "./openrouter-client";

// Set up command line argument parsing
const program = new Command();
program
  .name("chess-agent")
  .description("Lightweight TypeScript Chess Agent that communicates with Chess server via ZMQ and OpenRouter API")
  .requiredOption("--model <model>", "OpenRouter model name")
  .requiredOption("--name <name>", "Agent name")
  .requiredOption("--endpoint <endpoint>", "ZMQ port or endpoint URI")
  .requiredOption("--openrouter-api-key <key>", "OpenRouter API Key")
  .parse(process.argv);

const options = program.opts();

async function main() {
  const agentName = options.name;
  const modelName = options.model;
  const endpoint = options.endpoint;
  const apiKey = options["openrouter-api-key"] || options.openrouterApiKey; // handles both naming variants

  console.log(`[${agentName}] Starting agent client...`);
  console.log(`[${agentName}] Model: ${modelName}`);
  console.log(`[${agentName}] Connecting to: ${endpoint}`);

  const zmqClient = new ZmqClient(endpoint);
  const openRouterClient = new OpenRouterClient(apiKey, modelName);

  let myColor: "WHITE" | "BLACK" | null = null;
  let isGeneratingMove = false;
  let enableQuip = false;
  let enableDrawOffer = false;
  let enableResignation = false;
  let personality = "";

  const pendingRequests = new Map<string, (response: any) => void>();

  // Graceful shutdown handling
  const shutdown = async (exitCode = 0) => {
    console.log(`\n[${agentName}] Shutting down ZMQ connection...`);
    await zmqClient.close();
    process.exit(exitCode);
  };
  process.on("SIGINT", () => shutdown(0));
  process.on("SIGTERM", () => shutdown(0));

  const safeSend = async (msg: any) => {
    // console.log(`[ChessAgent(${myColor || agentName})] Sending Message: ${msg.type}`);
    await zmqClient.send(msg);
  };

  // Global error handlers to forward to server
  process.on("uncaughtException", async (err) => {
    console.error(`[${agentName}] Uncaught Exception:`, err);
    try {
      await safeSend({
        type: "ERROR",
        level: "FATAL",
        code: "UNCAUGHT_EXCEPTION",
        message: err.message || String(err)
      });
    } catch (sendErr) {
      console.error("Failed to send fatal error via ZMQ:", sendErr);
    }
    await shutdown(1);
  });

  process.on("unhandledRejection", async (reason) => {
    console.error(`[${agentName}] Unhandled Rejection:`, reason);
    try {
      await safeSend({
        type: "ERROR",
        level: "FATAL",
        code: "UNHANDLED_REJECTION",
        message: reason instanceof Error ? reason.message : String(reason)
      });
    } catch (sendErr) {
      console.error("Failed to send fatal error via ZMQ:", sendErr);
    }
    await shutdown(1);
  });

  try {
    await zmqClient.bind();

    // Start listening for messages
    for await (const message of zmqClient.receiveMessages()) {
      if (!message || typeof message.type !== "string") {
        console.warn(`[${agentName}] Received invalid or empty message:`, message);
        continue;
      }

      // console.log(`[ChessAgent(${myColor || agentName})] Received Message: ${message.type}`);

      switch (message.type.toLowerCase()) {
        case "ping":
          await safeSend({ type: "pong" });
          break;

        case "setup":
          myColor = message.color;
          enableQuip = message.enable_quip || false;
          enableDrawOffer = message.enable_draw_offer || false;
          enableResignation = message.enable_resignation || false;
          console.log(`[${agentName}] Setup color: ${myColor}, enableQuip: ${enableQuip}, enableDrawOffer: ${enableDrawOffer}, enableResignation: ${enableResignation}`);
          
          personality = await openRouterClient.generatePersonality(agentName, myColor || "WHITE");
          console.log(`[${agentName}] Generated personality: "${personality}"`);

          await safeSend({ 
            type: "setup_ack", 
            color: myColor, 
            model: modelName, 
            personality: personality 
          });
          break;

        case "start_move":
        case "game_history":
        case "error_recovery":
        case "draw_declined":
          if (isGeneratingMove) {
            console.warn(`[${agentName}] Move calculation already in progress. Ignoring.`);
            break;
          }
          const history = message.game_history || [];
          const quips = message.opponent_quips || [];
          const errors = message.errors || [];
          if (message.type === "draw_declined") {
            errors.push("Your draw offer was declined by the opponent. You MUST make a move now.");
          }
          const msgPersonality = message.personality || personality;
          const turnNumber = message.turn_number || (Math.floor(history.length / 2) + 1);
          handleMoveRequest(
            zmqClient, 
            openRouterClient, 
            history, 
            quips, 
            myColor || "WHITE", 
            agentName, 
            errors,
            msgPersonality,
            enableQuip,
            enableDrawOffer,
            enableResignation,
            turnNumber
          ).catch(err => console.error(`[${agentName}] Error in handleMoveRequest:`, err));
          break;

        case "draw_offer":
          const drawHistory = message.game_history || [];
          const drawQuips = message.opponent_quips || [];
          const drawPersonality = message.personality || personality;
          handleDrawOfferRequest(
            zmqClient,
            openRouterClient,
            drawHistory,
            drawQuips,
            drawPersonality,
            agentName
          ).catch(err => console.error(`[${agentName}] Error in handleDrawOfferRequest:`, err));
          break;

        case "end_game":
          console.log(`[${agentName}] Game over! Winner: ${message.winner}, Cause: ${message.cause}`);
          await shutdown();
          break;

        case "error":
          console.error(`[${agentName}] Received server error:`, message);
          break;

        case "retrospective_request":
          console.log(`[${agentName}] Received retrospective request`);
          const retroHistory = message.game_history || [];
          const retroQuips = message.opponent_quips || [];
          const retroPersonality = message.personality || personality;
          handleRetrospectiveRequest(
            zmqClient,
            openRouterClient,
            retroHistory,
            retroQuips,
            myColor || "WHITE",
            agentName,
            message.winner || "DRAW",
            message.cause || "UNKNOWN",
            retroPersonality
          ).catch(err => console.error(`[${agentName}] Error in handleRetrospectiveRequest:`, err));
          break;

        case "board_state_response":
          console.log(`[${agentName}] Received board state response:\n${JSON.stringify(message, null, 2)}`);
          if (pendingRequests.has(message.id)) {
            pendingRequests.get(message.id)!(message);
            pendingRequests.delete(message.id);
          }
          break;

        default:
          console.warn(`[${agentName}] Unknown message type: ${message.type}`, message);
      }
    }
  } catch (err) {
    console.error(`[${agentName}] Error in agent execution loop:`, err);
    await zmqClient.close();
    process.exit(1);
  }

  async function handleMoveRequest(
    zmq: ZmqClient,
    openRouter: OpenRouterClient,
    gameHistory: string[],
    opponentQuips: string[],
    color: "WHITE" | "BLACK",
    name: string,
    previousErrors: string[],
    currentPersonality: string,
    isQuipEnabled: boolean,
    isDrawOfferEnabled: boolean,
    isResignationEnabled: boolean,
    turnNumber: number
  ) {
    isGeneratingMove = true;
    try {
      const result = await openRouter.getNextMove(
        gameHistory,
        opponentQuips,
        color,
        previousErrors,
        currentPersonality,
        isQuipEnabled,
        isDrawOfferEnabled,
        isResignationEnabled,
        turnNumber,
        async (reasoningDelta, id) => {
          await safeSend({ type: "reasoning", id: id, message: reasoningDelta });
        },
        async (responseDelta, id) => {
          await safeSend({ type: "response", id: id, message: responseDelta });
        },
        async (quipMessage, id) => {
          await safeSend({ type: "quip", id: id, message: quipMessage });
        },
        async (id) => {
          return new Promise((resolve, reject) => {
            pendingRequests.set(id, resolve);
            safeSend({ type: "fetch_board_state", id: id }).catch((err) => {
              pendingRequests.delete(id);
              reject(err);
            });

            setTimeout(() => {
              if (pendingRequests.has(id)) {
                pendingRequests.delete(id);
                resolve({ error: "Timeout fetching board state" });
              }
            }, 10000);
          });
        }
      );

      if (result.type === "move") {
        console.log(`[${name}] Generated move decision: ${result.move}`);
        await safeSend({ type: "move_decision", algebraic_move_string: result.move });
      } else if (result.type === "offer_draw") {
        console.log(`[${name}] Offering draw`);
        await safeSend({ type: "offer_draw" });
      } else if (result.type === "resign") {
        console.log(`[${name}] Resigning`);
        await safeSend({ type: "resign" });
      }
    } catch (err) {
      console.error(`[${name}] Failed to generate decision:`, err);
      await safeSend({
        type: "ERROR",
        level: "CRITICAL",
        code: "AGENT_DECISION_FAILED",
        message: err instanceof Error ? err.message : String(err)
      });
    } finally {
      isGeneratingMove = false;
    }
  }

  async function handleDrawOfferRequest(
    zmq: ZmqClient,
    openRouter: OpenRouterClient,
    gameHistory: string[],
    opponentQuips: string[],
    currentPersonality: string,
    name: string
  ) {
    try {
      const accepted = await openRouter.getDrawDecision(
        gameHistory,
        opponentQuips,
        currentPersonality,
        async (reasoningDelta, id) => {
          await safeSend({ type: "reasoning", id: id, message: reasoningDelta });
        },
        async (responseDelta, id) => {
          await safeSend({ type: "response", id: id, message: responseDelta });
        }
      );

      console.log(`[${name}] Draw offer decision: ${accepted ? "ACCEPTED" : "DECLINED"}`);
      await safeSend({ type: "draw_decision", accept: accepted });
    } catch (err) {
      console.error(`[${name}] Failed to handle draw offer:`, err);
      // Default to declining if something goes wrong
      await safeSend({ type: "draw_decision", accept: false });
    }
  }

  async function handleRetrospectiveRequest(
    zmq: ZmqClient,
    openRouter: OpenRouterClient,
    gameHistory: string[],
    opponentQuips: string[],
    color: "WHITE" | "BLACK",
    name: string,
    winner: string,
    cause: string,
    currentPersonality: string
  ) {
    try {
      await openRouter.getRetrospectiveQuips(
        gameHistory,
        opponentQuips,
        winner,
        cause,
        currentPersonality,
        async (quipMessage, id) => {
          await safeSend({ type: "quip", id: id, message: quipMessage });
        }
      );

      await safeSend({ type: "end_turn", id: `${name}-retro-done` });
    } catch (err) {
      console.error(`[${name}] Failed to generate retrospective:`, err);
      await safeSend({ type: "end_turn", id: `${name}-retro-failed` });
    }
  }
}

main().catch(async (err) => {
  console.error("Critical failure in main:", err);
  process.exit(1);
});
