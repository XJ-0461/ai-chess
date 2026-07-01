import * as zmq from "zeromq";

async function runMockServer() {
  const socket = new zmq.Pair();
  const endpoint = "tcp://127.0.0.1:5555";
  
  console.log(`[MockServer] Binding to ZMQ Pair socket at ${endpoint}...`);
  await socket.bind(endpoint);
  console.log("[MockServer] Bound successfully. Waiting for client to connect...");

  // Give client a brief moment to connect, or send ping and wait.
  
  // Step 1: Send PING
  console.log("[MockServer] Sending PING...");
  await socket.send(JSON.stringify({ type: "ping" }));

  // Listen for messages
  for await (const [msg] of socket) {
    const message = JSON.parse(msg.toString());
    console.log(`[MockServer] Received:`, message);

    if (message.type === "pong") {
      console.log("[MockServer] Received PONG. Sending Setup...");
      await socket.send(JSON.stringify({ type: "setup", color: "WHITE" }));
    } 
    else if (message.type === "setup_ack") {
      console.log("[MockServer] Received SetupAck. Sending GameHistory to request first move...");
      await socket.send(JSON.stringify({ 
        type: "game_history", 
        game_history: ["e4", "e5", "Nf3", "Nc6"] 
      }));
    }
    else if (message.type === "reasoning") {
      process.stdout.write(`[Reasoning Stream] ${message.message}\n`);
    }
    else if (message.type === "response") {
      process.stdout.write(`[Response Stream] ${message.message}\n`);
    }
    else if (message.type === "move_decision") {
      console.log(`[MockServer] Move Decision Received: ${message.long_algebraic_move_string}`);
      console.log("[MockServer] Sending EndGame...");
      await socket.send(JSON.stringify({ 
        type: "end_game", 
        winner: "WHITE", 
        cause: "CHECKMATE" 
      }));
      break;
    }
    else if (message.type === "ERROR") {
      console.error("[MockServer] Received ERROR from agent:", message);
      console.log("[MockServer] Test finished (terminated due to agent error - expected if no api key).");
      break;
    }
  }

  console.log("[MockServer] Closing socket...");
  socket.close();
  console.log("[MockServer] Exited.");
}

runMockServer().catch(err => {
  console.error("[MockServer] Fatal error:", err);
});
