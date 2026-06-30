import { Command } from "commander";
import { ZmqClient } from "./zmq-client";
import { OpenRouterClient } from "./openrouter-client";

// Set up command line argument parsing
const program = new Command();
program
  .name("chess-agent")
  .description("Lightweight TypeScript Chess Agent that communicates with Chess server via ZMQ and OpenRouter API")
  .requiredOption("--name <name>", "Agent name")
  .requiredOption("--endpoint <endpoint>", "ZMQ port or endpoint URI")
  .option("--model <model>", "Fallback model id (normally provided in the setup command)")
  .option("--openrouter-api-key <key>", "Fallback OpenRouter API key (normally provided in the setup command)")
  .parse(process.argv);

const options = program.opts();

async function main() {
  const agentName = options.name;
  const endpoint = options.endpoint;
  // Provider/model/credentials now arrive in the setup command. CLI values are
  // optional fallbacks for manual testing only.
  const fallbackModel = options.model || "";
  const fallbackApiKey = options["openrouter-api-key"] || options.openrouterApiKey || "";

  console.log(`[${agentName}] Starting agent client...`);
  console.log(`[${agentName}] Connecting to: ${endpoint}`);
  console.log(`[${agentName}] Waiting for setup command (provider/model/credentials)...`);

  const zmqClient = new ZmqClient(endpoint);

  // Lazily created when the setup command arrives with provider details.
  let openRouterClient: OpenRouterClient | null = null;
  let modelName = "";

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

        case "setup": {
          myColor = message.color;
          enableQuip = message.enable_quip || false;
          enableDrawOffer = message.enable_draw_offer || false;
          enableResignation = message.enable_resignation || false;

          // The setup command carries the provider connection details so the
          // agent is configured at game time rather than at startup.
          const provider = (message.provider ?? "OpenRouter").toString();
          const requestedModel = message.model ?? message.model_id ?? fallbackModel;
          const apiKey = message.api_key ?? message.openrouter_api_key ?? fallbackApiKey;

          console.log(`[${agentName}] Setup color: ${myColor}, provider: ${provider}, model: ${requestedModel}, enableQuip: ${enableQuip}, enableDrawOffer: ${enableDrawOffer}, enableResignation: ${enableResignation}`);

          if (provider !== "OpenRouter") {
            const errorMessage = `Provider '${provider}' is not supported yet. Only OpenRouter is implemented.`;
            console.error(`[${agentName}] ${errorMessage}`);
            await safeSend({ type: "ERROR", level: "FATAL", code: "UNSUPPORTED_PROVIDER", message: errorMessage });
            await shutdown(1);
            break;
          }
          if (!apiKey) {
            const errorMessage = "OpenRouter setup is missing an API key.";
            console.error(`[${agentName}] ${errorMessage}`);
            await safeSend({ type: "ERROR", level: "FATAL", code: "MISSING_API_KEY", message: errorMessage });
            await shutdown(1);
            break;
          }
          if (!requestedModel) {
            const errorMessage = "OpenRouter setup is missing a model id.";
            console.error(`[${agentName}] ${errorMessage}`);
            await safeSend({ type: "ERROR", level: "FATAL", code: "MISSING_MODEL", message: errorMessage });
            await shutdown(1);
            break;
          }

          modelName = requestedModel;
          openRouterClient = new OpenRouterClient(apiKey, modelName);

          personality = await openRouterClient.generatePersonality(agentName, myColor || "WHITE");
          console.log(`[${agentName}] Generated personality: "${personality}"`);

          await safeSend({
            type: "setup_ack",
            color: myColor,
            model: modelName,
            personality: personality
          });
          break;
        }

        case "start_move":
        case "game_history":
        case "error_recovery":
        case "draw_declined":
          if (!openRouterClient) {
            console.error(`[${agentName}] Received ${message.type} before setup completed.`);
            await safeSend({ type: "ERROR", level: "CRITICAL", code: "NOT_CONFIGURED", message: "Received a game message before setup." });
            break;
          }
          if (isGeneratingMove) {
            console.warn(`[${agentName}] Move calculation already in progress. Ignoring.`);
            break;
          }
          const history = message.game_history || [];
          const quips = message.opponent_quips || [];
          const ownQuipHistory = message.own_quip_history || [];
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
            ownQuipHistory,
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
          if (!openRouterClient) {
            console.error(`[${agentName}] Received draw_offer before setup completed.`);
            break;
          }
          const drawHistory = message.game_history || [];
          const drawQuips = message.opponent_quips || [];
          const drawOwnQuipHistory = message.own_quip_history || [];
          const drawPersonality = message.personality || personality;
          handleDrawOfferRequest(
            zmqClient,
            openRouterClient,
            drawHistory,
            drawQuips,
            drawOwnQuipHistory,
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
          if (!openRouterClient) {
            console.error(`[${agentName}] Received retrospective_request before setup completed.`);
            break;
          }
          console.log(`[${agentName}] Received retrospective request`);
          const retroHistory = message.game_history || [];
          const retroQuips = message.opponent_quips || [];
          const retroOwnQuipHistory = message.own_quip_history || [];
          const retroPersonality = message.personality || personality;
          handleRetrospectiveRequest(
            zmqClient,
            openRouterClient,
            retroHistory,
            retroQuips,
            retroOwnQuipHistory,
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
    ownQuipHistory: string[],
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
        ownQuipHistory,
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
    ownQuipHistory: string[],
    currentPersonality: string,
    name: string
  ) {
    try {
      const accepted = await openRouter.getDrawDecision(
        gameHistory,
        opponentQuips,
        ownQuipHistory,
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
    ownQuipHistory: string[],
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
        ownQuipHistory,
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
