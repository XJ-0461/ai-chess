import { OpenRouter, tool, hasToolCall, ConversationState } from "@openrouter/agent";
import { z } from "zod";

/**
 * OpenRouterClient handles all interactions with the OpenRouter Agent SDK.
 * It is modeled strictly after the "Headless Agent" pattern found in the 
 * create-headless-agent skill samples.
 */
export class OpenRouterClient {
  private client: OpenRouter;
  private modelName: string;
  private conversationStore = new Map<string, ConversationState>();
  private conversationId = "chess-match";

  constructor(apiKey: string, modelName: string) {
    // NOTE: Pattern from create-headless-agent/sample/src/agent.ts (line 27)
    this.client = new OpenRouter({ apiKey });
    this.modelName = modelName;
  }

  private get stateAccessor() {
    return {
      load: async () => this.conversationStore.get(this.conversationId) ?? null,
      save: async (state: ConversationState) => { this.conversationStore.set(this.conversationId, state); }
    };
  }

  /**
   * Requests a move decision from the OpenRouter model.
   * Streams reasoning and message response deltas in real-time.
   * Uses the make_move tool to capture the final choice.
   */
  public async getNextMove(
    gameHistory: string[],
    color: "WHITE" | "BLACK",
    previousErrors: string[],
    onReasoning: (delta: string, id: string) => void,
    onResponse: (delta: string, id: string) => void
  ): Promise<string> {
    let chosenMove: string | null = null;

    // NOTE: Pattern from create-headless-agent/SKILL.md (Tool Pattern section)
    // and create-headless-agent/references/tools.md (Default-ON Tools section)
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

    const errorText = previousErrors.length > 0
      ? `\nPREVIOUS ATTEMPT ERRORS (You must fix these):\n${previousErrors.join("\n")}\n`
      : "";

    const userPrompt = `Game History so far:
${historyText}

Current Turn: ${color}
${errorText}
Please explain your reasoning and strategy, then make your next move by calling the \`make_move\` tool.`;

    const generateMovePromise = async (): Promise<string> => {
      console.log(`[OpenRouter] Initiating callModel for ${this.modelName}...`);
      
      // NOTE: Pattern from create-headless-agent/sample/src/agent.ts (runAgent function, callModel configuration)
      const result = this.client.callModel({
        model: this.modelName,
        instructions: systemPrompt,
        input: [{ role: "user", content: userPrompt }],
        tools: [makeMoveTool],
        state: this.stateAccessor,
        // NOTE: hasToolCall stop condition is described in create-headless-agent/SKILL.md (What @openrouter/agent handles section)
        stopWhen: [
          hasToolCall("make_move")
        ]
      });

      let accumulatedText = "";

      // NOTE: We rely exclusively on getItemsStream here to leverage the "Items" Paradigm 
      // and get unique item IDs, while preventing ReusableReadableStream deadlocks.
      for await (const item of result.getItemsStream()) {
        const id = item.id;
        if (item.type === "reasoning") {
          // NOTE: pattern from create-headless-agent/sample/src/agent.ts (line 85: extracting summary text)
          const text = item.summary?.map((s: { text: string }) => s.text).join("") ?? "";
          onReasoning(text, id);
        } else if (item.type === "message") {
          const text = typeof item.content === "string" ? item.content : "";
          if (text.length > accumulatedText.length) {
            accumulatedText = text;
            onResponse(text, id);
          }
        }
      }

      // If the stream finished but the tool wasn't invoked via execute, check outputText
      if (!chosenMove) {
        console.log("[OpenRouter] Tool execution not captured, checking final response...");
        // NOTE: Pattern from create-headless-agent/sample/src/agent.ts (line 93: getResponse)
        const response = await result.getResponse();
        // NOTE: Pattern from create-headless-agent/sample/src/agent.ts (line 96: check response.outputText)
        const text = accumulatedText || response.outputText || "";
        
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

      console.log(`[OpenRouter] Successfully captured move: ${chosenMove}`);
      return chosenMove;
    };

    try {
      // NOTE: We've removed the artificial 60s timeout to allow large models (550b+) 
      // or queued free endpoints sufficient time to process and respond.
      return await generateMovePromise();
    } catch (err) {
      console.error("OpenRouter client execution failed:", err);
      throw err;
    }
  }
}
