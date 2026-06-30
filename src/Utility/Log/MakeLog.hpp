#pragma once

#include <initializer_list>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

// MakeLog builds the structured JSON payload that is handed to spdlog. The
// logger's pattern (see Application logging setup) wraps this object with a
// timestamp and level so every emitted line is a single self-contained JSON
// record. Anything nlohmann::json can serialise is accepted as a field value.

namespace chess::log {

// One structured field: a key paired with any JSON-serialisable value.
using Field = std::pair<std::string, nlohmann::json>;

// Example:
//   MakeLog({ {"operation", "enter"}, {"function", "OnTurnTransition"} })
//     -> {"function":"OnTurnTransition","operation":"enter"}
[[nodiscard]] inline std::string MakeLog(std::initializer_list<Field> fields) {
    nlohmann::json record = nlohmann::json::object();
    for (const auto& [key, value] : fields) {
        record[key] = value;
    }
    return record.dump();
}

} // namespace chess::log
