import { OpenRouter, tool, hasToolCall } from "@openrouter/agent";
import { z } from "zod";

export class OpenRouterClient {
  private client: OpenRouter;
  private modelName: string;

  constructor(apiKey: string, modelName: string) {
    this.client = new OpenRouter({ apiKey });
    this.modelName = modelName;
  }

  /**
   * Requests a move decision from the OpenRouter model.
   * Streams reasoning and message response deltas in real-time.
   * Uses the make_move tool to capture the final choice.
   */
  public async getNextMove(
    gameHistory: string[],
    color: "WHITE" | "BLACK",
    onReasoning: (delta: string) => void,
    onResponse: (delta: string) => void
  ): Promise<string> {
    let chosenMove: string | null = null;

    // Define the tool for making the move
    const makeMoveTool = tool({
      name: "make_move",
      description: "Submit your chosen next move in algebraic notation.",
      inputSchema: z.object({
        algebraic_move_string: z.string().describe("The algebraic chess move, e.g. 'e4', 'Nf3', 'O-O', 'exd5', 'Qxd4+', 'e8=Q'"),
      }) as any,
      execute: async (params: any) => {
        chosenMove = params.algebraic_move_string;
        return { success: true, move: params.algebraic_move_string };
      },
    });

    // Format the game history
    const historyText = gameHistory.length > 0
      ? gameHistory.map((move, index) => {
          const moveNum = Math.floor(index / 2) + 1;
          const isWhite = index % 2 === 0;
          return isWhite ? `${moveNum}. ${move}` : `${move}`;
        }).join(" ")
      : "No moves have been played yet. It is the start of the game.";

    const systemPrompt = `You are a Grandmaster-level chess engine playing as ${color}.
You must evaluate the game history, decide your next best legal move, and output it by calling the \`make_move\` tool.
Explain your strategic reasoning, pawn structures, key threats, and short/long-term plans.
Do NOT write code or discuss implementation details. Just think and play chess.
Ensure the move you decide is a valid, legal chess move in standard algebraic notation.`;

    const userPrompt = `Game History so far:
${historyText}

Current Turn: ${color}

Please explain your reasoning and strategy, then make your next move by calling the \`make_move\` tool.`;

    // Track previously emitted text content for each item to compute deltas
    const lastSeenContents = new Map<string, string>();

    const generateMovePromise = async (): Promise<string> => {
      const result = this.client.callModel({
        model: this.modelName,
        input: [
          { role: "system", content: systemPrompt },
          { role: "user", content: userPrompt }
        ],
        tools: [makeMoveTool],
        stopWhen: [
          hasToolCall("make_move")
        ]
      });

      // Process the items stream to get reasoning and response updates
      for await (const item of result.getItemsStream()) {
        const itemId = item.id || item.type;
        if (item.type === "reasoning" && typeof item.content === "string") {
          const prev = lastSeenContents.get(itemId) || "";
          if (item.content.length > prev.length) {
            const delta = item.content.slice(prev.length);
            lastSeenContents.set(itemId, item.content);
            onReasoning(delta);
          }
        } else if (item.type === "message" && typeof item.content === "string") {
          const prev = lastSeenContents.get(itemId) || "";
          if (item.content.length > prev.length) {
            const delta = item.content.slice(prev.length);
            lastSeenContents.set(itemId, item.content);
            onResponse(delta);
          }
        }
      }

      // If the stream finished but the tool wasn't invoked, check if the model wrote the move in text
      if (!chosenMove) {
        const text = await result.getText();
        // Fallback parsing: look for make_move call or general move formats
        const regex = /make_move\s*\(\s*\{\s*algebraic_move_string:\s*["']([^"']+)["']/i;
        const match = text.match(regex);
        if (match && match[1]) {
          chosenMove = match[1];
        } else {
          // Look for brackets like [MOVE: e4] or similar
          const moveMatch = text.match(/\[MOVE:\s*([a-zA-Z0-9+#=-]+)\]/i);
          if (moveMatch && moveMatch[1]) {
            chosenMove = moveMatch[1];
          }
        }
      }

      if (!chosenMove) {
        throw new Error("Model finished but did not submit a move decision via make_move tool.");
      }

      return chosenMove;
    };

    const timeoutPromise = new Promise<never>((_, reject) => {
      setTimeout(() => {
        reject(new Error("OpenRouter API call timed out after 60 seconds."));
      }, 60000);
    });

    try {
      return await Promise.race([generateMovePromise(), timeoutPromise]);
    } catch (err) {
      console.error("OpenRouter client execution failed:", err);
      throw err;
    }
  }
}
