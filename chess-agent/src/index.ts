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
  const shutdown = async () => {
    console.log(`\n[${agentName}] Shutting down ZMQ connection...`);
    await zmqClient.close();
    process.exit(0);
  };
  process.on("SIGINT", shutdown);
  process.on("SIGTERM", shutdown);

  try {
    await zmqClient.connect();

    // Start listening for messages
    for await (const message of zmqClient.receiveMessages()) {
      if (!message || typeof message.type !== "string") {
        console.warn(`[${agentName}] Received invalid or empty message:`, message);
        continue;
      }

      switch (message.type.toLowerCase()) {
        case "ping":
          console.log(`[${agentName}] Received PING, responding with PONG`);
          await zmqClient.send({ type: "pong" });
          break;

        case "setup":
          myColor = message.color;
          console.log(`[${agentName}] Setup color: ${myColor}`);
          await zmqClient.send({ type: "setup_ack", color: myColor });
          break;

        case "start_move":
          console.log(`[${agentName}] Received start_move request. Triggering move calculation...`);
          if (isGeneratingMove) {
            console.warn(`[${agentName}] Move calculation already in progress. Ignoring.`);
            break;
          }
          await handleMoveRequest(zmqClient, openRouterClient, [], myColor || "WHITE", agentName);
          break;

        case "game_history":
          const history = message.game_history || [];
          console.log(`[${agentName}] Received game_history with ${history.length} moves. Calculating next move...`);
          if (isGeneratingMove) {
            console.warn(`[${agentName}] Move calculation already in progress. Ignoring.`);
            break;
          }
          await handleMoveRequest(zmqClient, openRouterClient, history, myColor || "WHITE", agentName);
          break;

        case "end_game":
          console.log(`[${agentName}] Game over! Winner: ${message.winner}, Cause: ${message.cause}`);
          await shutdown();
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
    name: string
  ) {
    isGeneratingMove = true;
    try {
      const decision = await openRouter.getNextMove(
        gameHistory,
        color,
        async (reasoningDelta) => {
          // Stream reasoning back to Chess server
          await zmq.send({ type: "reasoning", message: reasoningDelta });
        },
        async (responseDelta) => {
          // Stream assistant response back to Chess server
          await zmq.send({ type: "response", message: responseDelta });
        }
      );

      console.log(`[${name}] Generated move decision: ${decision}`);
      // Send move decision back to server to validate/apply
      await zmq.send({ type: "move_decision", algebraic_move_string: decision });
    } catch (err) {
      console.error(`[${name}] Failed to generate move decision:`, err);
      await zmq.send({
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

main().catch((err) => {
  console.error("Critical failure in main:", err);
  process.exit(1);
});
