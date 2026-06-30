#pragma once

#include <string>

namespace chess::application::model {

// Details for a single OpenRouter model, as surfaced by the public
// https://openrouter.ai/api/v1/models endpoint.
struct OpenRouterModelDetails {
    std::string id{};
    std::string name{};

    // Cost information. OpenRouter quotes pricing in USD per token. A negative
    // value (e.g. -1) is OpenRouter's sentinel for "not applicable / varies".
    double prompt_price_per_token{0.0};      // cost per input (prompt) token
    double completion_price_per_token{0.0};  // cost per output (completion) token

    long context_length{0};

    // Performance information. The /models endpoint does NOT expose these, so
    // they remain 0 (rendered as "-") until populated from a per-model stats
    // query. Kept here so the browser can already offer the columns.
    double throughput_tokens_per_second{0.0}; // higher is better
    double latency_seconds{0.0};              // p50 round-trip, lower is better
    double time_to_first_token_seconds{0.0};  // TTFT, lower is better
};

} // namespace chess::application::model
