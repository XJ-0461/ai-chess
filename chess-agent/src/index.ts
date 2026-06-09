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

  // Graceful shutdown handling
  const shutdown = async (exitCode = 0) => {
    console.log(`\n[${agentName}] Shutting down ZMQ connection...`);
    await zmqClient.close();
    process.exit(exitCode);
  };
  process.on("SIGINT", () => shutdown(0));
  process.on("SIGTERM", () => shutdown(0));

  const safeSend = async (msg: any) => {
    console.log(`[ChessAgent(${myColor || agentName})] Sending Message: ${msg.type}`);
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

      console.log(`[ChessAgent(${myColor || agentName})] Received Message: ${message.type}`);

      switch (message.type.toLowerCase()) {
        case "ping":
          await safeSend({ type: "pong" });
          break;

        case "setup":
          myColor = message.color;
          console.log(`[${agentName}] Setup color: ${myColor}`);
          await safeSend({ type: "setup_ack", color: myColor, model: modelName });
          break;

        case "start_move":
          if (isGeneratingMove) {
            console.warn(`[${agentName}] Move calculation already in progress. Ignoring.`);
            break;
          }
          await handleMoveRequest(zmqClient, openRouterClient, [], myColor || "WHITE", agentName, []);
          break;

        case "game_history":
          const history = message.game_history || [];
          if (isGeneratingMove) {
            console.warn(`[${agentName}] Move calculation already in progress. Ignoring.`);
            break;
          }
          await handleMoveRequest(zmqClient, openRouterClient, history, myColor || "WHITE", agentName, []);
          break;

        case "end_game":
          console.log(`[${agentName}] Game over! Winner: ${message.winner}, Cause: ${message.cause}`);
          await shutdown();
          break;

        case "error_recovery":
          console.log(`[${agentName}] Received error recovery message:`, message);
          if (isGeneratingMove) {
            console.warn(`[${agentName}] Move calculation already in progress. Ignoring.`);
            break;
          }
          const errors = message.errors || [];
          const currentHistory = message.game_history || [];
          await handleMoveRequest(zmqClient, openRouterClient, currentHistory, myColor || "WHITE", agentName, errors);
          break;

        case "error":
          console.error(`[${agentName}] Received server error:`, message);
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
    color: "WHITE" | "BLACK",
    name: string,
    previousErrors: string[]
  ) {
    isGeneratingMove = true;
    try {
      const decision = await openRouter.getNextMove(
        gameHistory,
        color,
        previousErrors,
        async (reasoningDelta, id) => {
          await safeSend({ type: "reasoning", id: id, message: reasoningDelta });
        },
        async (responseDelta, id) => {
          await safeSend({ type: "response", id: id, message: responseDelta });
        }
      );

      console.log(`[${name}] Generated move decision: ${decision}`);
      await safeSend({ type: "move_decision", algebraic_move_string: decision });
    } catch (err) {
      console.error(`[${name}] Failed to generate move decision:`, err);
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
}

main().catch(async (err) => {
  console.error("Critical failure in main:", err);
  process.exit(1);
});
