# Headless Chess Agent Implementation Plan

## 1. Problem Diagnosis: The "0 Request" Connection Failure

**Current Failure:**
- ZMQ messages flow from C++ to TS.
- TS agent calls `openRouter.getNextMove()`.
- **OpenRouter Dashboard:** 0 requests, 0 tokens.
- **Outcome:** 60s timeout in TS agent.

**Root Cause Analysis:**
The `@openrouter/agent` SDK performs local validation of the `callModel` parameters before initiating the network request. If this validation fails, it may throw an error that is trapped within the async iterator's internal state, or it may simply never start the underlying HTTP request if the stream is not being "pulled" correctly.

The current implementation has three critical flaws preventing the connection:
1.  **Invalid `input` format**: Passing a `[{role, content}]` array directly to `input` is not supported by the SDK's default `callModel` signature; it expects a `string` or the internal `Item[]` type. This causes a silent validation failure.
2.  **Stream Deadlock**: The SDK's internal agent loop is driven by the consumption of its output streams. If `getTextStream()` and `getItemsStream()` are not consumed in parallel, the internal state machine can hang, preventing the network request from ever being dispatched.
3.  **Missing `instructions` field**: The SDK separates "who I am" (`instructions`) from "what I should do now" (`input`). Mixing them or using them incorrectly leads to malformed requests that some models (especially via OpenRouter's abstraction) reject.

## 2. Implementation Strategy: "Cookbook-First" Architecture

We will refactor `openrouter-client.ts` to strictly follow the `create-headless-agent` and `long-horizon-agents` samples.

### A. Robust `callModel` Configuration
We will move from the "chat completion" style to the "agent instructions" style:
- **`instructions`**: Set to `systemPrompt`.
- **`input`**: Set to `userPrompt` (a plain string).
- This ensures the SDK can correctly wrap the request for the specific model.

### B. Parallel Stream Driving (The "Engine")
We will implement a concurrent consumption pattern using `Promise.all`. This is the only way to ensure the SDK's internal loop stays "hot" and keeps the network request alive.

```typescript
const streamText = async () => {
  for await (const delta of result.getTextStream()) {
    onResponse(delta); // Real-time response streaming
  }
};

const streamItems = async () => {
  for await (const item of result.getItemsStream()) {
    // Process reasoning items from item.summary
  }
};

await Promise.all([streamText(), streamItems()]);
```

### C. Connection Smoke Test & Logging
To stop the "idiot guessing," we will add explicit logging immediately before and after the `callModel` call to confirm the SDK has successfully initiated the request.

## 3. Detailed Code Specification (`chess-agent/src/openrouter-client.ts`)

```typescript
import { OpenRouter, tool, hasToolCall } from "@openrouter/agent";
import { z } from "zod";

export class OpenRouterClient {
  private client: OpenRouter;
  private modelName: string;

  constructor(apiKey: string, modelName: string) {
    this.client = new OpenRouter({ apiKey });
    this.modelName = modelName;
  }

  public async getNextMove(
    gameHistory: string[],
    color: "WHITE" | "BLACK",
    onReasoning: (delta: string) => void,
    onResponse: (delta: string) => void
  ): Promise<string> {
    let chosenMove: string | null = null;

    // 1. Tool Definition (Strict Zod Schema)
    const makeMoveTool = tool({
      name: "make_move",
      description: "Submit your chosen next move in algebraic notation.",
      inputSchema: z.object({
        algebraic_move_string: z.string().describe("The algebraic chess move, e.g. 'e4', 'Nf3', 'O-O', 'exd5', 'Qxd4+', 'e8=Q'"),
      }),
      execute: async ({ algebraic_move_string }) => {
        chosenMove = algebraic_move_string;
        return { success: true, move: algebraic_move_string };
      },
    });

    // 2. Prompt Construction
    const historyText = gameHistory.length > 0 
      ? gameHistory.map((m, i) => (i % 2 === 0 ? `${Math.floor(i/2)+1}. ${m}` : m)).join(" ")
      : "No moves have been played yet.";

    const systemPrompt = `You are a Grandmaster-level chess engine playing as ${color}. Output your move using the make_move tool.`;
    const userPrompt = `Game History: ${historyText}\n\nCurrent Turn: ${color}. Please make your move.`;

    // 3. The Generation Promise
    const generateMovePromise = async (): Promise<string> => {
      console.log(`[OpenRouter] Calling model: ${this.modelName}...`);
      
      const result = this.client.callModel({
        model: this.modelName,
        instructions: systemPrompt,
        input: userPrompt,
        tools: [makeMoveTool],
        stopWhen: [hasToolCall("make_move")]
      });

      let lastReasoningLength = 0;

      // DRIVE STREAMS IN PARALLEL
      const streamText = async () => {
        for await (const delta of result.getTextStream()) {
          onResponse(delta);
        }
      };

      const streamItems = async () => {
        for await (const item of result.getItemsStream()) {
          if (item.type === "reasoning") {
            const text = item.summary?.map((s: any) => s.text).join("") ?? "";
            if (text && text.length > lastReasoningLength) {
              const delta = text.slice(lastReasoningLength);
              lastReasoningLength = text.length;
              onReasoning(delta);
            }
          }
        }
      };

      await Promise.all([streamText(), streamItems()]);

      // 4. Final Verification
      if (!chosenMove) {
        const response = await result.getResponse();
        // Fallback to parsing response.outputText if tool didn't fire
        const text = response.outputText || "";
        const match = text.match(/\[MOVE:\s*([a-zA-Z0-9+#=-]+)\]/i);
        if (match) chosenMove = match[1];
      }

      if (!chosenMove) throw new Error("No move decision produced.");
      return chosenMove;
    };

    // 5. Execution with Timeout
    return await Promise.race([
      generateMovePromise(),
      new Promise<never>((_, reject) => setTimeout(() => reject(new Error("Timeout")), 60000))
    ]);
  }
}
```

## 4. Verification & Testing Loop
1. **Apply Edits**: Rewrite `openrouter-client.ts`.
2. **Rebuild**: `cd chess-agent && npm run build`.
3. **Execution**: Run `./scripts/start_chess_match.sh`.
4. **Validation**: Check OpenRouter dashboard for request activity *within seconds* of the `start_move` command.
