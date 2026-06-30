#pragma once

#include <string>

namespace chess::application::model {

// Details for a single AWS Bedrock model. Bedrock can enumerate models via its
// client, but pricing is not available from the API; on-demand pricing is the
// most important tier and will be populated separately. Stubbed for now so the
// cache variant and browser window can already accommodate it.
struct AwsBedrockModelDetails {
    std::string model_id{};
    std::string provider_name{};

    // On-demand pricing (USD per 1K tokens). Other subscription tiers may be
    // added later; on-demand is the critical one.
    double on_demand_input_price_per_1k_tokens{0.0};
    double on_demand_output_price_per_1k_tokens{0.0};
};

} // namespace chess::application::model
