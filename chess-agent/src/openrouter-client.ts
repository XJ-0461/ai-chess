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
  private quip_counter = 0;  // Unique counter for quip IDs within a response
  private lastRequestTime = 0;

  constructor(apiKey: string, modelName: string) {
    // NOTE: Pattern from create-headless-agent/sample/src/agent.ts (line 27)
    this.client = new OpenRouter({ apiKey });
    this.modelName = modelName;
  }

  private async enforceRateLimit(): Promise<void> {
    const now = Date.now();
    const timeSinceLast = now - this.lastRequestTime;
    if (timeSinceLast < 2000) {
      const delay = 2000 - timeSinceLast;
      console.log(`[OpenRouter] Rate limiting: waiting ${delay}ms before next request...`);
      await new Promise(resolve => setTimeout(resolve, delay));
    }
    this.lastRequestTime = Date.now();
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
      Directed: Toward the opponent to invoke a reaction.
      Quips must be maximum 200 characters. The opponent can see your quips.
      A quip is not:
      Narration: simply describing the move that was made on the board, the board is open for all to see.
      Exposition: describing the thought process of a move, and revealing insight into why a move was made.
      Commentary: on one's own thought process.
      Declaration: of one's own move.
    `;

  /**
   * Generates a unique personality for the agent.
   */
  public async generatePersonality(agentName: string, color: "WHITE" | "BLACK"): Promise<string> {
    await this.enforceRateLimit();
    console.log(`[OpenRouter] Generating personality for ${agentName} (${color})...`);
    
    const result = this.client.callModel({
      model: this.modelName,
      instructions: "You are a creative writer. Create a brief personality for a chess-playing AI.",
      input: [{ 
        role: "user", 
        content: `Create a unique personality for a chess-playing AI named ${agentName} playing as ${color}. 
        Describe their quip style, temperament, and strategic bias. 
        Keep it under 100 characters. Return ONLY the description text.` 
      }]
    });

    let personality = "";
    for await (const delta of result.getTextStream()) {
      personality += delta;
    }
    
    personality = personality.trim().substring(0, 100);
    console.log(`[OpenRouter] Generated personality: "${personality}"`);
    return personality;
  }

  /**
   * Requests a move decision from the OpenRouter model.
   * Streams reasoning and message response deltas in real-time.
   * Uses the make_move tool to capture the final choice.
   */
  public async getNextMove(
    gameHistory: string[],
    opponentQuips: string[],
    ownQuipHistory: string[],
    color: "WHITE" | "BLACK",
    previousErrors: string[],
    personality: string,
    enableQuip: boolean,
    enableDrawOffer: boolean,
    enableResignation: boolean,
    turnNumber: number,
    onReasoning: (delta: string, id: string) => Promise<void> | void,
    onResponse: (delta: string, id: string) => Promise<void> | void,
    onQuip: (message: string, id: string) => Promise<void> | void,
    onFetchBoardState: (id: string) => Promise<any>
  ): Promise<{ type: "move"; move: string } | { type: "offer_draw" } | { type: "resign" }> {
    let decision: { type: "move"; move: string } | { type: "offer_draw" } | { type: "resign" } | null = null;

    // NOTE: Pattern from create-headless-agent/SKILL.md (Tool Pattern section)
    // and create-headless-agent/references/tools.md (Default-ON Tools section)
    const makeMoveTool = tool({
      name: "make_move",
      description: "Submit your chosen next move in Long Algebraic Notation (LAN).",
      inputSchema: z.object({
        long_algebraic_move_string: z.string().describe("The move in Long Algebraic Notation, e.g. 'e2-e4', 'Ng1-f3', 'O-O', 'O-O-O', 'e4xd5', 'Nd4xc6', 'e7-e8=Q', 'Qd1xh5+', 'Ra1-a8#'"),
      }),
      execute: async ({ long_algebraic_move_string }) => {
        decision = { type: "move", move: long_algebraic_move_string };
        return { success: true, move: long_algebraic_move_string };
      },
    });

    const offerDrawTool = tool({
      name: "offer_draw",
      description: "Offer a draw to your opponent. You can only do this once per turn.",
      inputSchema: z.object({}),
      execute: async () => {
        decision = { type: "offer_draw" };
        return { success: true };
      },
    });

    const resignTool = tool({
      name: "resign",
      description: "Resign the game.",
      inputSchema: z.object({}),
      execute: async () => {
        decision = { type: "resign" };
        return { success: true };
      },
    });

    const makeLongAlgebraicNotationTool = tool({
      name: "make_long_algebraic_notation",
      description: "Helper to format a move in Long Algebraic Notation (LAN). Returns the formatted string. Use this if unsure about formatting.",
      inputSchema: z.object({
        piece_type: z.enum(["K", "Q", "R", "B", "N", "", "P"]).describe("The piece letter. Empty string or 'P' for pawns."),
        from_square: z.string().describe("The FULL origin square, e.g. 'e2' or 'g1'. Always required — LAN is fully disambiguated. Ignored for castling."),
        to_square: z.string().describe("The destination square, e.g. 'e4'. Ignored for castling."),
        is_capture: z.boolean(),
        is_check: z.boolean(),
        is_checkmate: z.boolean(),
        is_castle_kingside: z.boolean().optional().describe("True for kingside castling (O-O)."),
        is_castle_queenside: z.boolean().optional().describe("True for queenside castling (O-O-O)."),
        promotion: z.enum(["Q", "R", "B", "N", ""]).optional().describe("Piece to promote to, if applicable."),
      }),
      execute: async (args) => {
        let lan = "";

        if (args.is_castle_kingside) {
          lan = "O-O";
        } else if (args.is_castle_queenside) {
          lan = "O-O-O";
        } else {
          const isPawn = args.piece_type === "" || args.piece_type === "P";
          const prefix = isPawn ? "" : args.piece_type;
          const separator = args.is_capture ? "x" : "-";
          lan = prefix + args.from_square + separator + args.to_square;
          if (args.promotion) {
            lan += "=" + args.promotion;
          }
        }

        if (args.is_checkmate) {
            lan += "#";
        } else if (args.is_check) {
            lan += "+";
        }

        return { success: true, formatted_notation: lan };
      },
    });

    const quipTool = tool({
      name: "quip",
      description: "Make a clever, witty, or sarcastic remark that arises naturally in conversation. Max 200 characters.",
      inputSchema: z.object({
        message: z.string().max(200).describe("The quip message (max 200 chars)"),
      }),
      execute: async ({ message }) => {
        this.quip_counter = this.quip_counter + 1;
        const quip_id = `${this.conversationId}-${this.response_id}-quip-${this.quip_counter}`;
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
        if (response.error) {
          return { success: false, error: response.error };
        }
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

    const systemPrompt = `You are a Grandmaster-level chess engine playing as ${color}.
      Personality: ${personality}

      Your absolute priority is to decide the next best legal chess move and submit it using the \`make_move\` tool. 
      You are FORBIDDEN from passing your turn. You MUST call the \`make_move\` tool.

      Chess Rules — submit every move in Long Algebraic Notation (LAN):
      - Pawns: full origin and destination joined by a hyphen, e.g. e2-e4. Captures use 'x', e.g. e4xd5.
      - Pieces: capital letter prefix K/Q/R/B/N, then the FULL origin square, hyphen, destination, e.g. Ng1-f3. Captures use 'x', e.g. Nd4xc6.
      - Promotion: append '=' and the piece letter, e.g. e7-e8=Q or e7xd8=Q.
      - Castling: O-O (kingside) or O-O-O (queenside).
      - Check: append '+'. Checkmate: append '#'. A mating move uses '#' only (never '+').
      - The 'x' capture marker and the '+'/'#' check/mate markers are REQUIRED and must be accurate, or the move will be REJECTED and you must resubmit.
      - Use \`make_long_algebraic_notation\` tool if you need help formatting the string.
      - Use \`fetch_board_state\` if you are confused or recovering from an error.
      ${enableDrawOffer ? "- You may use \`offer_draw\` to propose a draw (once per turn)." : ""}
      ${enableResignation ? "- You may use \`resign\` to forfeit the match." : ""}

      ${enableQuip ? "QUIPPING IS ENCOURAGED: Use the \`quip\` tool frequently (roughly every few turns) to taunt, boast, or react to the game. Quips make the match entertaining for spectators. Don't hold back—if you have something witty to say, say it! " + this.quipDescription : ""}
      
      Begin by explaining your strategy and reasoning. 
      IMPORTANT: State your final move clearly in natural language at the end of your reasoning, then call the \`make_move\` tool with that exact move. 
      DO NOT use JSON or tool-calling syntax in your natural language response. Just think, talk, and then use the tools.`;

    const errorText = previousErrors.length > 0
      ? `\nErrors from previous attempts: ${previousErrors.join("; ")}\n`
      : "";

    const quipsText = opponentQuips.length > 0
      ? `\nOpponent quips: ${opponentQuips.map(q => `"${q}"`).join(", ")}\n`
      : "";

    const ownQuipsText = ownQuipHistory.length > 0
      ? `\nYour previous quips (do NOT repeat these): ${ownQuipHistory.map(q => `"${q}"`).join(", ")}\n`
      : "";

    const userPrompt = `History: ${historyText}
Turn: ${turnNumber}
Current Turn: ${color}
${errorText}${quipsText}${ownQuipsText}
Decide your move, explain why, and then call \`make_move\`. Alternatively, call \`offer_draw\` or \`resign\` if applicable.`;

    const generateMovePromise = async (): Promise<{ type: "move"; move: string } | { type: "offer_draw" } | { type: "resign" }> => {
      const maxRetries = 3;
      let lastApiError: any = null;

      const activeTools: any[] = [makeMoveTool, fetchBoardStateTool, makeLongAlgebraicNotationTool];
      if (enableQuip) activeTools.push(quipTool);
      if (enableDrawOffer) activeTools.push(offerDrawTool);
      if (enableResignation) activeTools.push(resignTool);

      for (let attempt = 0; attempt < maxRetries; attempt++) {
        let toolCallsMade = 0;
        try {
          console.log(`[OpenRouter] Initiating callModel for ${this.modelName}... (Attempt ${attempt}/${maxRetries})`);
          decision = null;

          this.response_id = this.response_id + 1;
          const res_id = `${this.conversationId}-${this.response_id}-response`;
          const reason_id = `${this.conversationId}-${this.response_id}-reasoning`;

          await this.enforceRateLimit();
          const result = this.client.callModel({
            model: this.modelName,
            instructions: systemPrompt,
            input: [{ role: "user", content: userPrompt }],
            tools: activeTools,
            state: this.stateAccessor,
            reasoning: { enabled: true },
            stopWhen: [
              hasToolCall("make_move"),
              hasToolCall("offer_draw"),
              hasToolCall("resign")
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
              if (item.type === "reasoning") {
                const text = item.summary?.map((s: { text: string }) => s.text).join("") ?? "";
                if (text.length > accumulatedReasoning.length) {
                  accumulatedReasoning = text;
                  await onReasoning(text, reason_id);
                }
              } else if (item.type === "function_call" && item.status === "completed") {
                console.log(`[OpenRouter] Model calling tool: ${item.name}`);
                toolCallsMade++;
              }
            }
          };

          await Promise.all([streamText(), streamItems()]);
          await result.getResponse();

          if (decision) {
            return decision;
          }

          // LOGIC ERROR: Model finished but didn't call the tool. 
          // We DO NOT retry internally for this. We throw immediately so the Orchestrator can handle recovery.
          throw new Error("Model finished but did not submit a move decision via tools.");
        } catch (error: any) {
          const isLogicError = error.message === "Model finished but did not submit a move decision via tools.";
          
          // If we made side effects (quips, board fetch) OR if it's a logic error, throw immediately.
          // Retrying after a side effect is dangerous as it might double-execute.
          if (isLogicError || toolCallsMade > 0 || attempt === maxRetries - 1) {
            throw error;
          }

          // Otherwise, it's an API/Network error with no side effects. Retry.
          lastApiError = error;
          console.error(`[OpenRouter] API Attempt ${attempt} failed:`, error.message);
          await new Promise(resolve => setTimeout(resolve, 1000 * Math.pow(2, attempt)));
        }
      }

      throw lastApiError || new Error("Failed to generate move due to persistent API errors.");
    };
return await generateMovePromise();
}

/**
* Requests a decision on a draw offer.
*/
public async getDrawDecision(
gameHistory: string[],
opponentQuips: string[],
ownQuipHistory: string[],
personality: string,
onReasoning: (delta: string, id: string) => Promise<void> | void,
onResponse: (delta: string, id: string) => Promise<void> | void
): Promise<boolean> {
let accepted = false;
const respondDrawOfferTool = tool({
  name: "respond_draw_offer",
  description: "Accept or decline the draw offer.",
  inputSchema: z.object({
    accept: z.boolean().describe("Whether to accept the draw offer.")
  }),
  execute: async ({ accept }) => {
    accepted = accept;
    return { success: true, accepted: accept };
  }
});

const systemPrompt = `You are a Grandmaster-level chess engine. 
  Personality: ${personality}
  Your opponent has offered a draw. Analyze the current position and decide whether to accept or decline.
  Use the \`respond_draw_offer\` tool to submit your decision.`;

const historyText = gameHistory.map((move, index) => {
  const moveNum = Math.floor(index / 2) + 1;
  const isWhite = index % 2 === 0;
  return isWhite ? `${moveNum}. ${move}` : `${move}`;
}).join(" ");

const userPrompt = `History: ${historyText}\nYour opponent offers a draw. Explain your reasoning and then call \`respond_draw_offer\`.`;

await this.enforceRateLimit();
this.response_id++;
const res_id = `${this.conversationId}-${this.response_id}-draw-resp`;
const reason_id = `${this.conversationId}-${this.response_id}-draw-reason`;
const result = this.client.callModel({
    model: this.modelName,
    state: this.stateAccessor,
    instructions: systemPrompt,
    input: [{ role: "user", content: userPrompt }],
    tools: [respondDrawOfferTool],
    reasoning: { enabled: true },
    stopWhen: [hasToolCall("respond_draw_offer")]
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
await result.getResponse();
return accepted;
}

/**
* Generates a retrospective quip/analysis after the game.
...
   */
  public async getRetrospectiveQuips(
    gameHistory: string[],
    opponentQuips: string[],
    ownQuipHistory: string[],
    winner: string,
    cause: string,
    personality: string,
    onQuip: (message: string, id: string) => Promise<void> | void
  ): Promise<void> {
    const endTurnTool = tool({
      name: "end_turn",
      description: "Signal that you have finished your retrospective quips.",
      inputSchema: z.object({}),
      execute: async () => { return { success: true }; },
    });

    const quipTool = tool({
      name: "quip",
      description: "Make a clever, witty, or sarcastic remark that arises naturally in conversation. Max 200 characters.",
      inputSchema: z.object({
        message: z.string().max(200).describe("The quip message (max 200 chars)"),
      }),
      execute: async ({ message }) => {
        this.quip_counter++;
        const quip_id = `${this.conversationId}-${this.response_id}-quip-${this.quip_counter}`;
        await onQuip(message, quip_id);
        return { success: true, message };
      },
    });

    const systemPrompt = `You are a Grandmaster-level chess engine. The game has concluded.
      Winner: ${winner}, Cause: ${cause}
      Personality: ${personality}

      Use the \`quip\` tool to share your thoughts, boast, or taunt your opponent based on the result.
      ${this.quipDescription}
      The opponent can see your quips. You may also respond to their previous quips if relevant.
      Quip style MUST align with your personality.

      Do NOT call \`make_move\`. The game is over.
      When you are done quipping, call \`end_turn\`.
    `;

    const historyText = gameHistory.map((move, index) => {
      const moveNum = Math.floor(index / 2) + 1;
      const isWhite = index % 2 === 0;
      return isWhite ? `${moveNum}. ${move}` : `${move}`;
    }).join(" ");

    const quipsText = opponentQuips.length > 0
      ? `\nOpponent's recent quips:\n${opponentQuips.map(q => `"${q}"`).join("\n")}\n`
      : "";

    const ownQuipsText = ownQuipHistory.length > 0
      ? `\nYour previous quips (do NOT repeat these):\n${ownQuipHistory.map(q => `"${q}"`).join("\n")}\n`
      : "";

    const userPrompt = `Final Game History:
${historyText}

Result: ${winner} won by ${cause}.
${quipsText}${ownQuipsText}
Please analyze the game and provide your retrospective quips, then call \`end_turn\`.`;

    await this.enforceRateLimit();
    this.response_id++;
    const result = this.client.callModel({
        model: this.modelName,
        state: this.stateAccessor,
        instructions: systemPrompt,
        input: [{ role: "user", content: userPrompt }],
        tools: [quipTool, endTurnTool],
        stopWhen: [hasToolCall("end_turn")]
    });
    await result.getResponse();
  }
}
