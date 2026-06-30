#pragma once

#include <mutex>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "Application/Model/OpenRouterModelDetails.hpp"
#include "Application/Model/AwsBedrockModelDetails.hpp"

namespace chess::application::model {

// One cached model, keyed (logically) by "<Provider>:<model_id>".
using ModelDetails = std::variant<OpenRouterModelDetails, AwsBedrockModelDetails>;

// Stable cache key: "OpenRouter:<id>" / "AWSBedrock:<model_id>".
[[nodiscard]] inline std::string CacheKey(const ModelDetails& details) {
    return std::visit([](const auto& d) -> std::string {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, OpenRouterModelDetails>) {
            return "OpenRouter:" + d.id;
        } else {
            return "AWSBedrock:" + d.model_id;
        }
    }, details);
}

// Shared cache of fetched model details. Populated by TaskExecutor workers and
// read by the UI thread, so all access must be guarded by `mutex`.
struct ModelBrowserCache {
    std::mutex mutex;
    std::vector<ModelDetails> entries;
};

} // namespace chess::application::model
