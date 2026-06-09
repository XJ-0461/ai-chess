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
  private response_id = 0;

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

  private quipDescription: string = `
      A quip is a clever, witty, or sarcastic remark that is usually made on the spur of the moment. It functions as a sharp retort, a one-liner, or a droll observation that arises naturally in conversation.
      A quip is:
      Spontaneous: Quips sound offhand and unrehearsed, even if they are actually well-timed,
      Brief: Unlike a traditional joke with a setup and a punchline, a quip is a quick zinger,
      Humorous or Taunting: While often used for harmless comedic effect, a quip can sometimes carry a bitingly sarcastic or mocking edge.
      Quips must be maximum 200 characters. The opponent can see your quips.
    `;

  /**
   * Requests a move decision from the OpenRouter model.
   * Streams reasoning and message response deltas in real-time.
   * Uses the make_move tool to capture the final choice.
   */
  public async getNextMove(
    gameHistory: string[],
    opponentQuips: string[],
    color: "WHITE" | "BLACK",
    previousErrors: string[],
    onReasoning: (delta: string, id: string) => Promise<void> | void,
    onResponse: (delta: string, id: string) => Promise<void> | void,
    onQuip: (message: string, id: string) => Promise<void> | void,
    onFetchBoardState: (id: string) => Promise<any>
  ): Promise<string> {
    let chosenMove: string | null = null;

    // NOTE: Pattern from create-headless-agent/SKILL.md (Tool Pattern section)
    // and create-headless-agent/references/tools.md (Default-ON Tools section)
    const makeMoveTool = tool({
      name: "make_move",
      description: "Submit your chosen next move in algebraic notation.",
      inputSchema: z.object({
        algebraic_move_string: z.string().describe("The algebraic chess move, e.g. 'e4', 'Nf3', 'O-O', 'exd5', 'Qxd4+', 'e8(Q)'"),
      }),
      execute: async ({ algebraic_move_string }) => {
        chosenMove = algebraic_move_string;
        return { success: true, move: algebraic_move_string };
      },
    });

    const quipTool = tool({
      name: "quip",
      description: "Make a clever, witty, or sarcastic remark that arises naturally in conversation. Max 200 characters.",
      inputSchema: z.object({
        message: z.string().max(200).describe("The quip message (max 200 chars)"),
      }),
      execute: async ({ message }) => {
        const quip_id = `${this.conversationId}-${this.response_id}-quip`;
        await onQuip(message, quip_id);
        return { success: true, message };
      },
    });

    const fetchBoardStateTool = tool({
      name: "fetch_board_state",
      description: "Fetch the current board state including FEN and active pieces. Use this only when confused or recovering from an error.",
      inputSchema: z.object({}),
      execute: async () => {
        const req_id = `${this.conversationId}-${this.response_id}-fetch`;
        const response = await onFetchBoardState(req_id);
        return { success: true, data: response };
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

    const systemPrompt = `
      You are a Grandmaster-level chess engine playing as ${color}.
      You must evaluate the game history, decide your next best legal move, and output it by calling the \`make_move\` tool.
      You can optionally use the \`quip\` tool to make a short, witty, or sarcastic remark before or alongside making your move.
      You can optionally use the \`fetch_board_state\` tool to get the current state of the board if you are confused or recovering from an error.
      The quip tool should be used sparingly and only when it arises naturally in conversation (e.g. after a blunder by the opponent, or when you have a particularly strong move, or when you want to complement yourself, boast, or taunt the opponent).
      ${this.quipDescription}
      Do not feel obligated to respond to each and every quip by the opponent, quip when it suits you or makes you feel some type of way!
      At the start of the match, before your first move is played, determine your personality that will determine your quip style, temperament, and frequency. Persist this style throughout the match.
      Explain your strategic reasoning, pawn structures, key threats, and short/long-term plans.
      Do NOT write code or discuss implementation details. Just think and play chess.
      Ensure the move you decide is a valid, legal chess move in standard algebraic notation.
      Use 'O-O' for kingside castling and 'O-O-O' for queenside castling.
      When a pawn promotes, the piece promoted to is indicated at the end. For example, a pawn on e7 promoting to a queen on e8 may be variously rendered as e8Q, e8(Q)
    `;

    const errorText = previousErrors.length > 0
      ? `\nPREVIOUS ATTEMPT ERRORS (You must fix these, remember you have access to fetch_board_state tool if necessary):\n${previousErrors.join("\n")}\n`
      : "";

    const quipsText = opponentQuips.length > 0
      ? `\nOpponent's recent quips:\n${opponentQuips.map(q => `"${q}"`).join("\n")}\n`
      : "";

    const userPrompt = `Game History so far:
${historyText}

Current Turn: ${color}
${errorText}${quipsText}
Please explain your reasoning and strategy, then make your next move by calling the \`make_move\` tool. You may also use the \`quip\` tool or \`fetch_board_state\` tool if needed.`;

    const generateMovePromise = async (): Promise<string> => {
      const maxRetries = 3;
      let lastError: any = null;

      for (let attempt = 0; attempt < maxRetries; attempt++) {
        try {
          console.log(`[OpenRouter] Initiating callModel for ${this.modelName}... (Attempt ${attempt}/${maxRetries})`);
          chosenMove = null; // Reset on retry

          this.response_id = this.response_id + 1;
          const res_id = `${this.conversationId}-${this.response_id}-response`;
          const reason_id = `${this.conversationId}-${this.response_id}-reasoning`;
          const move_id = `${this.conversationId}-${this.response_id}-move`;

          // NOTE: Pattern from create-headless-agent/sample/src/agent.ts (runAgent function, callModel configuration)
          const result = this.client.callModel({
            model: this.modelName,
            instructions: systemPrompt,
            input: [{ role: "user", content: userPrompt }],
            tools: [makeMoveTool, quipTool, fetchBoardStateTool],
            state: this.stateAccessor,
            reasoning: { enabled: true },
            // NOTE: hasToolCall stop condition is described in create-headless-agent/SKILL.md (What @openrouter/agent handles section)
            stopWhen: [
              hasToolCall("make_move")
            ]
          });

          let accumulatedText = "";
          let accumulatedReasoning = "";

          const streamText = async () => {
            for await (const delta of result.getTextStream()) {
              accumulatedText += delta;
              await onResponse(accumulatedText, res_id);
            }
          };

          const streamItems = async () => {
            for await (const item of result.getItemsStream()) {
              const id = item.id;
              if (item.type === "reasoning") {
                const text = item.summary?.map((s: { text: string }) => s.text).join("") ?? "";
                if (text.length > accumulatedReasoning.length) {
                  accumulatedReasoning = text;
                  await onReasoning(text, reason_id);
                }
              }
            }
          };

          await Promise.all([streamText(), streamItems()]);

          // If the stream finished but the tool wasn't invoked via execute, check outputText
          // console.log("[OpenRouter] Tool execution not captured, checking final response...");
          // // NOTE: Pattern from create-headless-agent/sample/src/agent.ts (line 93: getResponse)
          // const response = await result.getResponse();
          // // NOTE: Pattern from create-headless-agent/sample/src/agent.ts (line 96: check response.outputText)
          // const text = accumulatedText || response.outputText || "";
          //
          // // Fallback parsing: look for make_move call or general move formats
          // const regex = /make_move\s*\(\s*\{\s*algebraic_move_string:\s*["']([^"']+)["']/i;
          // const match = text.match(regex);
          // if (match && match[1]) {
          //   chosenMove = match[1];
          // } else {
          //   // Look for brackets like [MOVE: e4] or similar
          //   const moveMatch = text.match(/\[MOVE:\s*([a-zA-Z0-9+#=-]+)\]/i);
          //   if (moveMatch && moveMatch[1]) {
          //     chosenMove = moveMatch[1];
          //   }
          // }

          // the model may call this, it should not be null if the model followed instructions and called the tool correctly.
          if (!chosenMove) {
            throw new Error("Model finished but did not submit a move decision via make_move tool.");
          }

          console.log(`[OpenRouter] Successfully captured move: ${chosenMove}`);
          return chosenMove;
        } catch (err: any) {
          lastError = err;
          console.error(`[OpenRouter] Attempt ${attempt} failed:`, err.message || err);
          if (attempt < maxRetries) {
            const delay = attempt * 2000;
            console.log(`[OpenRouter] Retrying in ${delay}ms...`);
            await new Promise(resolve => setTimeout(resolve, delay));
          }
        }
      }

      throw lastError || new Error("Failed to generate move after retries.");
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

  /**
   * Requests a game retrospective analysis and banter from the model.
   * Uses the end_turn tool to finish the retrospective round.
   */
  public async getRetrospective(
    gameHistory: string[],
    opponentQuips: string[],
    color: "WHITE" | "BLACK",
    winner: string,
    cause: string,
    onReasoning: (delta: string, id: string) => Promise<void> | void,
    onResponse: (delta: string, id: string) => Promise<void> | void,
    onQuip: (message: string, id: string) => Promise<void> | void
  ): Promise<void> {
    const endTurnTool = tool({
      name: "end_turn",
      description: "Signal that you are finished with your retrospective analysis and banter for this round.",
      inputSchema: z.object({}),
      execute: async () => {
        return { success: true };
      },
    });

    const quipTool = tool({
      name: "quip",
      description: "Make a clever, witty, or sarcastic remark about the game. Max 200 characters.",
      inputSchema: z.object({
        message: z.string().max(200).describe("The quip message (max 200 chars)"),
      }),
      execute: async ({ message }) => {
        const quip_id = `${this.conversationId}-${this.response_id}-quip`;
        await onQuip(message, quip_id);
        return { success: true, message };
      },
    });

    const historyText = gameHistory.map((move, index) => {
      const moveNum = Math.floor(index / 2) + 1;
      const isWhite = index % 2 === 0;
      return isWhite ? `${moveNum}. ${move}` : `${move}`;
    }).join(" ");

    const systemPrompt = `You are a Grandmaster-level chess engine. The game has concluded.
      RESULT: ${winner} won by ${cause}. (If result is DRAW, it was a STALEMATE).
      You are playing as ${color}.
      Analyze the game history, identify critical blunders, brilliant moves, or pivotal moments.
      Use the \`quip\` tool to share your thoughts, boast, or taunt your opponent based on the result.
      ${this.quipDescription}
      The opponent can see your quips. You may also respond to their previous quips if relevant.
      When you are finished with your analysis and banter for this turn, you MUST call the \`end_turn\` tool.
      Do NOT call \`make_move\`. The game is over.
    `;

    const quipsText = opponentQuips.length > 0
      ? `\nOpponent's recent quips:\n${opponentQuips.map(q => `"${q}"`).join("\n")}\n`
      : "";

    const userPrompt = `Final Game History:
${historyText}

The game is over. Winner: ${winner}, Cause: ${cause}.
${quipsText}
Please analyze the game and provide your retrospective quips, then call \`end_turn\`.`;

    try {
      this.response_id++;
      const res_id = `${this.conversationId}-${this.response_id}-retrospective-response`;
      const reason_id = `${this.conversationId}-${this.response_id}-retrospective-reasoning`;

      const result = this.client.callModel({
        model: this.modelName,
        instructions: systemPrompt,
        input: [{ role: "user", content: userPrompt }],
        tools: [quipTool, endTurnTool],
        state: this.stateAccessor,
        reasoning: { enabled: true },
        stopWhen: [hasToolCall("end_turn")]
      });

      let accumulatedText = "";
      let accumulatedReasoning = "";

      const streamText = async () => {
        for await (const delta of result.getTextStream()) {
          accumulatedText += delta;
          await onResponse(accumulatedText, res_id);
        }
      };

      const streamItems = async () => {
        for await (const item of result.getItemsStream()) {
          if (item.type === "reasoning") {
            const text = item.summary?.map((s: { text: string }) => s.text).join("") ?? "";
            if (text.length > accumulatedReasoning.length) {
              accumulatedReasoning = text;
              await onReasoning(text, reason_id);
            }
          }
        }
      };

      await Promise.all([streamText(), streamItems()]);
    } catch (err) {
      console.error("[OpenRouter] Retrospective failed:", err);
    }
  }
}
