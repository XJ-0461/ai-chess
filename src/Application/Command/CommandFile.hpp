#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "Application/Command/Command.hpp"

namespace chess::application::command {

// Parses a single command from its `type` string and `detail` object into a
// Command. Shared by the startup command file and the runtime ZMQ command
// server so both accept the same command surface. On an unknown/invalid type
// returns nullopt and writes a human-readable reason to `error`.
std::optional<Command> ParseCommand(const std::string& type, const nlohmann::json& detail, std::string& error);

// Loads and parses a startup command file of the form
// { "commands": [ { "type": ..., "detail": { ... } }, ... ] }.
// On failure returns nullopt and writes a human-readable reason to `error`.
std::optional<CommandList> LoadCommandsFromFile(const std::string& path, std::string& error);

} // namespace chess::application::command
