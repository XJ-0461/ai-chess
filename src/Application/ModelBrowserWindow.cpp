#include "ModelBrowserWindow.h"

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

#include <imgui.h>

namespace chess::application {

namespace {

// Flattened, provider-agnostic view of a cached model for display/sorting.
struct ModelRow {
    std::string provider{};
    std::string id{};      // model identifier (used for copy)
    std::string label{};   // display name (falls back to id)
    double input_price{0.0};
    double output_price{0.0};
    long context_length{0};
    double throughput{0.0};
    double latency{0.0};
    double time_to_first_token{0.0};
};

enum class SortMode : int {
    kPriceAscending = 0,
    kPriceDescending = 1,
    kThroughputDescending = 2,
};

enum class Column {
    kProvider,
    kModel,
    kInputPrice,
    kOutputPrice,
    kContext,
    kThroughput,
    kLatency,
    kTimeToFirstToken,
};

[[nodiscard]] double CombinedPrice(const ModelRow& row) {
    return row.input_price + row.output_price;
}

// OpenRouter quotes USD per token; humans read USD per million tokens. A
// negative value is OpenRouter's "varies / not applicable" sentinel.
[[nodiscard]] std::string FormatPricePerMillion(const double per_token) {
    if (per_token < 0.0) {
        return "-";
    }
    if (per_token == 0.0) {
        return "Free";
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "$%.4g", per_token * 1'000'000.0);
    return buffer;
}

[[nodiscard]] std::string FormatRate(const double tokens_per_second) {
    if (tokens_per_second <= 0.0) {
        return "-";
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.0f tok/s", tokens_per_second);
    return buffer;
}

[[nodiscard]] std::string FormatSeconds(const double seconds) {
    if (seconds <= 0.0) {
        return "-";
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2fs", seconds);
    return buffer;
}

[[nodiscard]] const char* HeaderOf(const Column column) {
    switch (column) {
    case Column::kProvider:         return "Provider";
    case Column::kModel:            return "Model";
    case Column::kInputPrice:       return "Input ($/M tok)";
    case Column::kOutputPrice:      return "Output ($/M tok)";
    case Column::kContext:          return "Context";
    case Column::kThroughput:       return "Throughput";
    case Column::kLatency:          return "Latency";
    case Column::kTimeToFirstToken: return "TTFT";
    }
    return "";
}

[[nodiscard]] std::string CellOf(const Column column, const ModelRow& row) {
    switch (column) {
    case Column::kProvider:         return row.provider;
    case Column::kModel:            return row.label.empty() ? row.id : row.label;
    case Column::kInputPrice:       return FormatPricePerMillion(row.input_price);
    case Column::kOutputPrice:      return FormatPricePerMillion(row.output_price);
    case Column::kContext:          return row.context_length > 0 ? std::to_string(row.context_length) : "-";
    case Column::kThroughput:       return FormatRate(row.throughput);
    case Column::kLatency:          return FormatSeconds(row.latency);
    case Column::kTimeToFirstToken: return FormatSeconds(row.time_to_first_token);
    }
    return "";
}

[[nodiscard]] std::vector<Column> VisibleColumns(const model::ModelBrowserColumns& columns) {
    std::vector<Column> visible;
    if (columns.provider)            { visible.push_back(Column::kProvider); }
    if (columns.model)               { visible.push_back(Column::kModel); }
    if (columns.input_price)         { visible.push_back(Column::kInputPrice); }
    if (columns.output_price)        { visible.push_back(Column::kOutputPrice); }
    if (columns.context_length)      { visible.push_back(Column::kContext); }
    if (columns.throughput)          { visible.push_back(Column::kThroughput); }
    if (columns.latency)             { visible.push_back(Column::kLatency); }
    if (columns.time_to_first_token) { visible.push_back(Column::kTimeToFirstToken); }
    return visible;
}

} // namespace

void RenderModelBrowserColumnsMenu(model::ModelBrowserColumns& columns) {
    ImGui::MenuItem("Provider", nullptr, &columns.provider);
    ImGui::MenuItem("Model", nullptr, &columns.model);
    ImGui::MenuItem("Input price", nullptr, &columns.input_price);
    ImGui::MenuItem("Output price", nullptr, &columns.output_price);
    ImGui::MenuItem("Context length", nullptr, &columns.context_length);
    ImGui::MenuItem("Throughput", nullptr, &columns.throughput);
    ImGui::MenuItem("Latency", nullptr, &columns.latency);
    ImGui::MenuItem("TTFT", nullptr, &columns.time_to_first_token);
}

void RenderModelBrowserWindow(
    bool* show,
    model::ModelBrowserCache& cache,
    model::IModelBrowser& open_router_browser,
    const model::ModelBrowserColumns& columns
) {
    if (!*show) {
        return;
    }

    if (ImGui::Begin("Model Browser", show)) {
        if (ImGui::Button("Fetch OpenRouter Models")) {
            open_router_browser.FetchModels();
        }

        ImGui::SameLine();

        static SortMode sort_mode = SortMode::kPriceAscending;
        static const char* const kSortLabels[] = {
            "Price (low to high)",
            "Price (high to low)",
            "Throughput (high to low)",
        };
        int sort_index = static_cast<int>(sort_mode);
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::Combo("Sort", &sort_index, kSortLabels, IM_ARRAYSIZE(kSortLabels))) {
            sort_mode = static_cast<SortMode>(sort_index);
        }

        // Snapshot the cache under lock, then sort/render without holding it.
        std::vector<ModelRow> rows;
        {
            const std::lock_guard<std::mutex> lock(cache.mutex);
            rows.reserve(cache.entries.size());
            for (const auto& entry : cache.entries) {
                std::visit([&rows](const auto& details) {
                    using T = std::decay_t<decltype(details)>;
                    if constexpr (std::is_same_v<T, model::OpenRouterModelDetails>) {
                        rows.push_back(ModelRow{
                            .provider = "OpenRouter",
                            .id = details.id,
                            .label = details.name,
                            .input_price = details.prompt_price_per_token,
                            .output_price = details.completion_price_per_token,
                            .context_length = details.context_length,
                            .throughput = details.throughput_tokens_per_second,
                            .latency = details.latency_seconds,
                            .time_to_first_token = details.time_to_first_token_seconds,
                        });
                    } else {
                        rows.push_back(ModelRow{
                            .provider = "AWS Bedrock",
                            .id = details.model_id,
                            .label = details.model_id,
                            .input_price = details.on_demand_input_price_per_1k_tokens,
                            .output_price = details.on_demand_output_price_per_1k_tokens,
                            .context_length = 0,
                            .throughput = 0.0,
                            .latency = 0.0,
                            .time_to_first_token = 0.0,
                        });
                    }
                }, entry);
            }
        }

        switch (sort_mode) {
        case SortMode::kPriceAscending:
            std::ranges::sort(rows, [](const ModelRow& a, const ModelRow& b) { return CombinedPrice(a) < CombinedPrice(b); });
            break;
        case SortMode::kPriceDescending:
            std::ranges::sort(rows, [](const ModelRow& a, const ModelRow& b) { return CombinedPrice(a) > CombinedPrice(b); });
            break;
        case SortMode::kThroughputDescending:
            std::ranges::sort(rows, [](const ModelRow& a, const ModelRow& b) { return a.throughput > b.throughput; });
            break;
        }

        ImGui::Text("%d models", static_cast<int>(rows.size()));

        const std::vector<Column> visible = VisibleColumns(columns);
        if (visible.empty()) {
            ImGui::TextDisabled("No columns selected (View > Model Browser > Configure > Columns).");
        } else {
            constexpr ImGuiTableFlags kTableFlags =
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
            if (ImGui::BeginTable("##models", static_cast<int>(visible.size()), kTableFlags)) {
                for (const Column column : visible) {
                    ImGui::TableSetupColumn(HeaderOf(column));
                }
                ImGui::TableHeadersRow();

                for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
                    const ModelRow& row = rows[row_index];
                    ImGui::PushID(static_cast<int>(row_index));
                    ImGui::TableNextRow();
                    for (std::size_t column_index = 0; column_index < visible.size(); ++column_index) {
                        ImGui::TableNextColumn();
                        const std::string cell = CellOf(visible[column_index], row);
                        if (column_index == 0) {
                            // First cell carries the whole-row selectable + context menu.
                            ImGui::Selectable(cell.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
                            if (ImGui::BeginPopupContextItem()) {
                                const std::string copy_label = "Copy model name \"" + row.id + "\"";
                                if (ImGui::MenuItem(copy_label.c_str())) {
                                    ImGui::SetClipboardText(row.id.c_str());
                                }
                                ImGui::EndPopup();
                            }
                        } else {
                            ImGui::TextUnformatted(cell.c_str());
                        }
                    }
                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
        }
    }
    ImGui::End();
}

} // namespace chess::application
