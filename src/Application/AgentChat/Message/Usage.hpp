#pragma once
#include <cstddef>
#include <nlohmann/json.hpp>

namespace chess::agent::message {

// Pricing expressed as a ratio so any "cost per N tokens" convention can be
// represented. USD-per-token == cost / per_tokens. OpenRouter reports price
// per single token, so the agent sends per_tokens == 1.
struct CostPerToken {
    double cost{0.0};
    double per_tokens{-1.0};
};

inline void from_json(const nlohmann::json& j, CostPerToken& c) {
    c.cost = j.value("cost", 0.0);
    c.per_tokens = j.value("per_tokens", 1.0);
}

struct ModelPricing {
    CostPerToken input;
    CostPerToken output;
};

inline void from_json(const nlohmann::json& j, ModelPricing& m) {
    m.input = j.value("input", CostPerToken{});
    m.output = j.value("output", CostPerToken{});
}

// Token usage reported by the agent once per turn (inbound only).
struct ModelUsage {
    static constexpr const char* kTypeTag = "model_usage";
    std::size_t input_tokens{0};
    std::size_t output_tokens{0};
};

inline void from_json(const nlohmann::json& j, ModelUsage& m) {
    m.input_tokens = j.value("input_tokens", std::size_t{0});
    m.output_tokens = j.value("output_tokens", std::size_t{0});
}

// Accumulated USD spend for a whole session, split by token direction.
struct SessionCostEstimate {
    double input_token_spend{0.0};
    double output_token_spend{0.0};
};

// Accumulates every reported turn's tokens against the captured pricing.
// per_tokens <= 0 means pricing was never reported, so that direction
// contributes nothing rather than dividing by the unset sentinel (-1).
// Templated on the range so this header stays free of any concurrent-vector
// (TBB) dependency — it accepts std::vector, tbb::concurrent_vector, etc.
template <typename UsageRange>
inline SessionCostEstimate EstimateSessionCost(const ModelPricing& pricing, const UsageRange& usage) {
    SessionCostEstimate estimate;
    for (const auto& u : usage) {
        if (pricing.input.per_tokens > 0.0) {
            estimate.input_token_spend += static_cast<double>(u.input_tokens) * pricing.input.cost / pricing.input.per_tokens;
        }
        if (pricing.output.per_tokens > 0.0) {
            estimate.output_token_spend += static_cast<double>(u.output_tokens) * pricing.output.cost / pricing.output.per_tokens;
        }
    }
    return estimate;
}

} // namespace chess::agent::message