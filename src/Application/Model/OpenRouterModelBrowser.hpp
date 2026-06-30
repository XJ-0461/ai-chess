#pragma once

#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <so_5/all.hpp>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include "Application/Model/IModelBrowser.hpp"
#include "Application/Model/ModelBrowserCache.hpp"
#include "Application/Task/BlockingTask.hpp"

namespace chess::application::model {

// Fetches the OpenRouter model catalogue (the public /models endpoint needs no
// API key) on the shared TaskExecutor pool, parses it, and replaces the
// OpenRouter entries in the shared cache.
class OpenRouterModelBrowser final : public IModelBrowser {
public:
    OpenRouterModelBrowser(so_5::environment_t& env, std::shared_ptr<ModelBrowserCache> cache)
        : env_{env},
          cache_{std::move(cache)} {}

    void FetchModels() override {
        const so_5::mbox_t executor = env_.create_mbox(std::string{task::kBlockingTaskMboxName});
        // Capture a shared_ptr so the cache outlives the job regardless of the
        // browser's lifetime, and run the blocking HTTP request off-thread.
        auto cache = cache_;
        so_5::send<task::BlockingTask>(executor, [cache]() { FetchInto(*cache); });
    }

private:
    static constexpr std::string_view kModelsEndpoint = "https://openrouter.ai/api/v1/models";

    static double ParsePrice(const nlohmann::json& pricing, const char* field) {
        if (!pricing.contains(field) || !pricing.at(field).is_string()) {
            return 0.0;
        }
        try {
            return std::stod(pricing.at(field).get<std::string>());
        } catch (...) {
            return 0.0;
        }
    }

    static void FetchInto(ModelBrowserCache& cache) {
        const cpr::Response response = cpr::Get(cpr::Url{std::string{kModelsEndpoint}});
        if (response.status_code != 200) {
            std::cerr << "OpenRouter model fetch failed (HTTP " << response.status_code << ")\n";
            return;
        }

        nlohmann::json json;
        try {
            json = nlohmann::json::parse(response.text);
        } catch (const std::exception& e) {
            std::cerr << "OpenRouter model fetch: JSON parse error: " << e.what() << "\n";
            return;
        }

        if (!json.contains("data") || !json.at("data").is_array()) {
            return;
        }

        std::vector<ModelDetails> fetched;
        fetched.reserve(json.at("data").size());
        for (const auto& model : json.at("data")) {
            OpenRouterModelDetails details;
            details.id = model.value("id", std::string{});
            details.name = model.value("name", std::string{});
            details.context_length = model.value("context_length", 0L);
            if (model.contains("pricing")) {
                const auto& pricing = model.at("pricing");
                details.prompt_price_per_token = ParsePrice(pricing, "prompt");
                details.completion_price_per_token = ParsePrice(pricing, "completion");
            }
            fetched.emplace_back(std::move(details));
        }

        const std::lock_guard<std::mutex> lock(cache.mutex);
        // Replace any existing OpenRouter entries with the freshly fetched set.
        std::erase_if(cache.entries, [](const ModelDetails& d) {
            return std::holds_alternative<OpenRouterModelDetails>(d);
        });
        cache.entries.insert(
            cache.entries.end(),
            std::make_move_iterator(fetched.begin()),
            std::make_move_iterator(fetched.end())
        );
    }

    so_5::environment_t& env_;
    std::shared_ptr<ModelBrowserCache> cache_;
};

} // namespace chess::application::model
